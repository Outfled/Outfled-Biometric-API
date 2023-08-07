#include "pch.h"
#include "image_manager.h"
#include "outfledbio_error.h"
#include <opencv2/opencv.hpp>
#include <python.h>
#include <Shlwapi.h>
#include <strsafe.h>

#pragma comment(lib, "Mfuuid.lib")
#pragma comment(lib, "MF.lib")
#pragma comment(lib, "MFplat.lib")
#pragma comment(lib, "Mfreadwrite.lib")
#pragma comment(lib, "Shlwapi.lib")
#ifdef _DEBUG
#pragma comment (lib,"opencv_world470d.lib")
#else
#pragma comment(lib, "opencv_world470.lib")
#endif

#define IMAGE_BIT_COUNT			((WORD)32 ) /* RGB-32 */
#define IMAGE_FRAME_WIDTH		1920
#define IMAGE_FRAME_HEIGHT		1080

/* Number of images needed for enrollments */
#define ENROLLMENT_IMAGE_COUNT	3

template<typename T>
void SafeRelease( T **ppObj )
{
	if ( ppObj && *ppObj )
	{
		( *ppObj )->Release();
		*ppObj = nullptr;
	}
}

CImageManager::CImageManager()
{
	CHAR		szPathBuffer[512];
	PyObject	*pSysPath;
	PyObject	*pModule;

	m_ppDeviceArray			= nullptr;
	m_nDeviceCount			= 0;
	m_pSelectedDevice		= nullptr;
	m_pCamSource			= nullptr;
	m_pSourceReader			= nullptr;
	m_pTrainerFunction		= nullptr;
	m_pClassifierFunction	= nullptr;

	Py_Initialize();

	//
	// Load OpenCV haar cascades
	//
	GetFullPathNameA( "data\\haarcascade_frontalface_alt.xml", 512, szPathBuffer, NULL );
	m_pFaceClassifier = new cv::CascadeClassifier( szPathBuffer );
	
	GetFullPathNameA( "data\\haarcascade_eye_tree_eyeglasses.xml", 512, szPathBuffer, NULL );
	m_pEyesClassifier = new cv::CascadeClassifier( szPathBuffer );

	//
	// Load the face trainer & classifer functions
	//
	pSysPath = PySys_GetObject( "path" );
	GetFullPathNameA( "modules/face_recognition", 512, szPathBuffer, NULL );
	PyList_Append( pSysPath, PyUnicode_FromString( szPathBuffer ) );

	pModule = PyImport_ImportModule( "recognizer" );
	if ( pModule )
	{
		m_pTrainerFunction		= PyObject_GetAttrString( pModule, "create_known_face" );
		m_pClassifierFunction	= PyObject_GetAttrString( pModule, "validate_face" );
	}
}

CImageManager::~CImageManager()
{
	CancelEnrollment();
	
	SafeRelease( &m_pSourceReader );
	SafeRelease( &m_pCamSource );
	if ( m_pSelectedDevice ) {
		m_ppDeviceArray[0]->ShutdownObject();
	}

	ClearDeviceArray();
}

HRESULT CImageManager::Refresh()
{
	IMFAttributes	*pCameraAttributes;
	HRESULT			hResult;

	ClearDeviceArray();

	//
	// Create camera attributes 
	//
	hResult = MFCreateAttributes( &pCameraAttributes, 1 );
	if ( hResult != S_OK ) {
		return hResult;
	}

	pCameraAttributes->SetGUID( MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID );

	/* Get array of camera devices */
	hResult = MFEnumDeviceSources( pCameraAttributes, &m_ppDeviceArray, &m_nDeviceCount );
	pCameraAttributes->Release();

	return hResult;
}

BOOL CImageManager::EnumerateCameras( DWORD dwIndex, LPWSTR *ppszCameraName )
{
	if ( dwIndex < m_nDeviceCount )
	{
		UINT32	cchFriendlyName;
		LPWSTR	lpszFriendlyName;
		HRESULT hResult;

		/* Get the device friendly name */
		hResult = m_ppDeviceArray[dwIndex]->GetAllocatedString( MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
			&lpszFriendlyName,
			&cchFriendlyName
		);
		if ( FAILED( hResult ) || !lpszFriendlyName ) {
			return FALSE;
		}

		*ppszCameraName = new WCHAR[cchFriendlyName + 1];
		wcscpy_s( *ppszCameraName, cchFriendlyName + 1, lpszFriendlyName );

		CoTaskMemFree( lpszFriendlyName );
		return TRUE;
	}

	return FALSE;
}

DWORD CImageManager::GetCameraCount() const
{
	return m_nDeviceCount;
}

HRESULT CImageManager::SelectDevice( DWORD dwIndex )
{
	HRESULT			hResult;
	IMFAttributes	*pCameraAttributes;
	DWORD			dwFlags;
	IMFSample		*pNullSample;

	if ( !m_ppDeviceArray ) {
		return E_FAIL;
	}
	if ( dwIndex >= m_nDeviceCount ) {
		return HRESULT_FROM_WIN32( ERROR_INDEX_OUT_OF_BOUNDS );
	}

	/* Create IMFMediaSource object for the selected device */
	hResult = (*m_ppDeviceArray)[dwIndex].ActivateObject( IID_PPV_ARGS( &m_pCamSource ) );
	if ( hResult != S_OK )
	{
		return hResult;
	}

	m_pSelectedDevice = m_ppDeviceArray[dwIndex];

	//
    // Set the camera attributes
    //
	MFCreateAttributes( &pCameraAttributes, 1 );
	pCameraAttributes->SetUINT32( MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE );
	pCameraAttributes->SetUINT32( MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE );

	/* Create the camera reader object */
	hResult = MFCreateSourceReaderFromMediaSource( m_pCamSource, pCameraAttributes, &m_pSourceReader );
	pCameraAttributes->Release();

	if ( hResult != S_OK )
	{
		SafeRelease( &m_pCamSource );
		
		m_pSelectedDevice->ShutdownObject();
		m_pSelectedDevice = nullptr;

		return hResult;
	}

	/* Set the output media type */
	hResult = SetCameraMediaType();
	if ( hResult != S_OK )
	{
		SafeRelease( &m_pSourceReader );
		SafeRelease( &m_pCamSource );

		m_pSelectedDevice->ShutdownObject();
		m_pSelectedDevice = nullptr;

		return hResult;
	}

	/* Prepare the camera for reading */
	m_pSourceReader->ReadSample( MF_SOURCE_READER_FIRST_VIDEO_STREAM,
		0,
		NULL,
		&dwFlags,
		NULL,
		&pNullSample
	);
	if ( pNullSample ) {
		pNullSample->Release();
	}

	return S_OK;
}

HRESULT CImageManager::StartEnrollment()
{
	if ( !m_pCamSource || !m_pSourceReader ) {
		return E_FAIL;
	}

	/* Enrollment already in progress */
	if ( m_rgEnrollmentImages.size() > 0 )
	{
		return HRESULT_FROM_WIN32( ERROR_ALREADY_INITIALIZED );
	}

	return S_OK;
}

HRESULT CImageManager::CaptureEnrollmentSample( ULONGLONG *pRejectReason )
{
	HRESULT hResult;
	HANDLE	hImageFile;
	CHAR	szImageFilePath[MAX_PATH];

	*pRejectReason = 0;

	if ( m_rgEnrollmentImages.size() >= ENROLLMENT_IMAGE_COUNT )
	{
		return OUTFLEDBIO_E_ENROLLMENT_COMPLETE;
	}

	/* Create the image output file */
	hImageFile = CreateTempImageFile( szImageFilePath );
	if ( hImageFile == INVALID_HANDLE_VALUE )
	{
		return HRESULT_FROM_WIN32( GetLastError() );
	}

	/* Take snapshot of the camera and save as image */
	hResult = TakeCameraSnapshot( hImageFile );
	if ( hResult != S_OK )
	{
		CloseHandle( hImageFile );
		DeleteFileA( szImageFilePath );

		return hResult;
	}

	/* Validate the sample image */
	CloseHandle( hImageFile );
	hResult = ValidateImageSample( szImageFilePath, pRejectReason );
	
	if ( hResult == S_OK )
	{
		LPSTR lpszPathString = new CHAR[strlen( szImageFilePath ) + 1];
		strcpy_s( lpszPathString, strlen( szImageFilePath ) + 1, szImageFilePath );

		m_rgEnrollmentImages.push_back( lpszPathString );
		hResult = ( m_rgEnrollmentImages.size() < ENROLLMENT_IMAGE_COUNT ) ? OUTFLEDBIO_I_MORE_DATA : S_OK;
	}
	else
	{
		DeleteFileA( szImageFilePath );
	}

	return hResult;
}

HRESULT CImageManager::CommitEnrollment( LPCWSTR lpszName )
{
	HRESULT		hResult;
	PyObject	*pArguments;
	CHAR		szOutputPath[MAX_PATH];
	CHAR		szOutputFileName[512];
	DWORD		cbAnsiString;
	LPSTR		lpszAnsiString;

	if ( m_rgEnrollmentImages.size() < ENROLLMENT_IMAGE_COUNT )
	{
		return OUTFLEDBIO_E_NO_DATA;
	}

	/* Create name for the output file */
	GetFullPathNameA( "trained", ARRAYSIZE( szOutputPath ), szOutputPath, NULL );
	for ( int i = 1;; ++i )
	{
		StringCbPrintfA( szOutputFileName, sizeof( szOutputFileName ), "%s\\encoded_face_%d", szOutputPath, i );
		if ( !PathFileExistsA( szOutputFileName ) )
		{
			break;
		}
	}

	/* Convert the name to ANSI */
	cbAnsiString	= WideCharToMultiByte( CP_UTF8, 0, lpszName, -1, NULL, 0, NULL, NULL );
	lpszAnsiString	= new CHAR[cbAnsiString];
	WideCharToMultiByte( CP_UTF8, 0, lpszName, -1, lpszAnsiString, cbAnsiString, NULL, NULL );

	/* Create arugments for the trainer function */
	pArguments = PyTuple_Pack( 5,
		PyUnicode_FromString( m_rgEnrollmentImages[0] ),
		PyUnicode_FromString( m_rgEnrollmentImages[1] ),
		PyUnicode_FromString( m_rgEnrollmentImages[2] ),
		PyUnicode_FromString( szOutputFileName ),
		PyUnicode_FromString( lpszAnsiString )
	);
	delete[] lpszAnsiString;

	/* Train the face */
	PyObject_CallObject( (PyObject *)m_pTrainerFunction, pArguments );
	if ( !PathFileExistsA( szOutputFileName ) )
	{
		return OUTFLEDBIO_E_UNKNOWN_ERROR;
	}

	/* Delete the image files */
	for ( size_t i = 0; i < m_rgEnrollmentImages.size(); ++i )
	{
		DeleteFileA( m_rgEnrollmentImages[i] );
		delete[] m_rgEnrollmentImages[i];
	}

	m_rgEnrollmentImages.clear();
	return S_OK;
}

HRESULT CImageManager::CancelEnrollment()
{
	/* Find each enrollment sample image */
	for ( DWORD i = 0; i < m_rgEnrollmentImages.size(); ++i )
	{
		DeleteFileA( m_rgEnrollmentImages[i] );
		delete[] m_rgEnrollmentImages[i];
	}

	m_rgEnrollmentImages.clear();
	return S_OK;
}

HRESULT CImageManager::IdentifySample( ULONGLONG *pRejectReason )
{
	HRESULT	hResult;
	CHAR	szTrainedFiles[MAX_PATH];
	HANDLE	hImageFile;
	CHAR	szImageFilePath[MAX_PATH];

	*pRejectReason = 0;

	/* Create the image output file */
	hImageFile = CreateTempImageFile( szImageFilePath );
	if ( hImageFile == INVALID_HANDLE_VALUE )
	{
		return HRESULT_FROM_WIN32( GetLastError() );
	}

	/* Take snapshot of the camera and save as image */
	hResult = TakeCameraSnapshot( hImageFile );
	if ( hResult != S_OK )
	{
		CloseHandle( hImageFile );
		DeleteFileA( szImageFilePath );

		return hResult;
	}

	CloseHandle( hImageFile );

	/* Validate the sample image */
	hResult = ValidateImageSample( szImageFilePath, pRejectReason );
	if ( hResult != S_OK )
	{
		DeleteFileA( szImageFilePath );
		return hResult;
	}

	hResult = OUTFLEDBIO_E_USER_NOT_FOUND;

	/* Enumerate the trained files */
	GetFullPathNameA( "trained", ARRAYSIZE( szTrainedFiles ), szTrainedFiles, NULL );
	for ( int i = 1;; ++i )
	{
		PyObject	*pArguments;
		PyObject	*pResult;
		CHAR		szTrainedFaceFile[MAX_PATH];

		StringCbPrintfA( szTrainedFaceFile, sizeof( szTrainedFaceFile ), "%s\\encoded_face_%d", szTrainedFiles, i );
		if ( !PathFileExistsA( szTrainedFaceFile ) )
		{
			break;
		}

		/* Set the python function arguments */
		pArguments = PyTuple_Pack( 2,
			PyUnicode_FromString( szImageFilePath ),
			PyUnicode_FromString( szTrainedFaceFile )
		);

		/* Identify the face */
		pResult = PyObject_CallObject( (PyObject *)m_pClassifierFunction, pArguments );
		if ( pResult && true == PyObject_IsTrue( pResult ) )
		{
			hResult = S_OK;
			break;
		}
	}

	DeleteFileA( szImageFilePath );
	return hResult;
}

VOID CImageManager::ClearDeviceArray()
{
	if ( m_ppDeviceArray )
	{
		for ( UINT32 i = 0; i < m_nDeviceCount; ++i ) {
			( *m_ppDeviceArray )[i].Release();
		}

		CoTaskMemFree( m_ppDeviceArray );

		m_ppDeviceArray = nullptr;
		m_nDeviceCount = 0;
	}
}

HRESULT CImageManager::SetCameraMediaType()
{
	HRESULT			hResult;
	IMFMediaType	*pMediaType;

	/* Create output media type */
	hResult = MFCreateMediaType( &pMediaType );
	if ( hResult != S_OK ) {
		return hResult;
	}

	//
	// Set the type to video, the format to RGB, and the image frame dimensions
	//
	hResult = pMediaType->SetGUID( MF_MT_MAJOR_TYPE, MFMediaType_Video );
	if ( SUCCEEDED( hResult ) ) {
		hResult = pMediaType->SetGUID( MF_MT_SUBTYPE, MFVideoFormat_RGB32 );
	}
	if ( SUCCEEDED( hResult ) ) {
		hResult = MFSetAttributeSize( pMediaType, MF_MT_FRAME_SIZE, IMAGE_FRAME_WIDTH, IMAGE_FRAME_HEIGHT );
	}
	
	if ( SUCCEEDED( hResult ) )
	{
		hResult = m_pSourceReader->SetCurrentMediaType( 0, 0, pMediaType );
	}

	pMediaType->Release();
	return hResult;
}

HANDLE CImageManager::CreateTempImageFile( LPSTR lpszFilePath )
{
	CHAR szTempPath[MAX_PATH + 1] = { 0 };
	
	GetTempPathA( ARRAYSIZE( szTempPath ), szTempPath );
	GetTempFileNameA( szTempPath, NULL, 0, lpszFilePath );

	return CreateFileA( lpszFilePath, GENERIC_ALL, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
}

HRESULT CImageManager::TakeCameraSnapshot( HANDLE hOutputFile )
{
	IMFSample *pSample;
	DWORD				dwSampleFlags;
	HRESULT				hResult;
	IMFMediaBuffer *pBuffer;
	PBYTE				pbImageBytes;
	DWORD				cbImageBytes;
	BITMAPFILEHEADER	bFileHeader;
	BITMAPINFOHEADER	bInfoHeader;
	BOOL				bWritten;

	ZeroMemory( &bFileHeader, sizeof( BITMAPFILEHEADER ) );
	ZeroMemory( &bInfoHeader, sizeof( BITMAPINFOHEADER ) );

	/* Get image from camera device */
	hResult = m_pSourceReader->ReadSample( MF_SOURCE_READER_FIRST_VIDEO_STREAM,
		0,
		NULL,
		&dwSampleFlags,
		NULL,
		&pSample
	);
	if ( hResult != S_OK )
	{
		return hResult;
	}

	//
	// Acquire the image bytes
	//
	hResult = pSample->ConvertToContiguousBuffer( &pBuffer );
	if ( hResult != S_OK )
	{
		SafeRelease( &pSample );
		return hResult;
	}

	hResult = pBuffer->Lock( &pbImageBytes, NULL, &cbImageBytes );
	if ( hResult != S_OK )
	{
		SafeRelease( &pBuffer );
		SafeRelease( &pSample );
		return hResult;
	}

	//
	// Set the file bitmap data
	//
	bFileHeader.bfType			= (WORD)( 'B' | ( 'M' << 8 ) );
	bFileHeader.bfOffBits		= sizeof( BITMAPFILEHEADER ) + sizeof( BITMAPINFOHEADER );
	bInfoHeader.biSize			= sizeof( BITMAPINFOHEADER );
	bInfoHeader.biBitCount		= IMAGE_BIT_COUNT;
	bInfoHeader.biCompression	= BI_RGB;
	bInfoHeader.biPlanes		= 1;
	bInfoHeader.biWidth			= IMAGE_FRAME_WIDTH;
	bInfoHeader.biHeight		= -( IMAGE_FRAME_HEIGHT );	/* Negative height prevents the image from being saved upside down */

	//
	// Write the image to the file
	//
	bWritten = WriteFile( hOutputFile, &bFileHeader, sizeof( BITMAPFILEHEADER ), NULL, NULL );
	if ( bWritten ) {
		bWritten &= WriteFile( hOutputFile, &bInfoHeader, sizeof( BITMAPINFOHEADER ), NULL, NULL );
	}
	if ( bWritten ) {
		bWritten &= WriteFile( hOutputFile, pbImageBytes, cbImageBytes, NULL, NULL );
	}
	if ( !bWritten ) {
		hResult = HRESULT_FROM_WIN32( GetLastError() );
	}

	//
	// Cleanup
	//
	pBuffer->Unlock();
	SafeRelease( &pBuffer );
	SafeRelease( &pSample );

	return hResult;
}

HRESULT CImageManager::ValidateImageSample( LPCSTR lpszFilePath, ULONGLONG *pRejectReason )
{
	using namespace cv;

	Mat					mFaceImage;
	std::vector<Rect>	rgFaceRects;
	std::vector<Rect>	rgEyeRects;
	CascadeClassifier	*pFaceClassifier;
	CascadeClassifier	*pEyesClassifier;

	pFaceClassifier = (CascadeClassifier *)m_pFaceClassifier;
	pEyesClassifier = (CascadeClassifier *)m_pEyesClassifier;

	if ( pFaceClassifier->empty() || pEyesClassifier->empty() )
	{
		return E_FAIL;
	}

	/* Read & load the face image file */
	mFaceImage = imread( lpszFilePath );
	if ( mFaceImage.empty() )
	{
		return OUTFLEDBIO_E_UNKNOWN_ERROR;
	}

	/* Attempt to detect the face */
	pFaceClassifier->detectMultiScale( mFaceImage, rgFaceRects, 1.1, 2, CASCADE_SCALE_IMAGE, Size( 100, 100 ) );
	if ( rgFaceRects.size() == 0 )
	{
		*pRejectReason = ( rgFaceRects.size() == 0 ) ? OTFBIO_RJR_FF_FACE_NOT_FOUND : OTFBIO_RJR_FF_MULTIPLE_FACES;
		return OUTFLEDBIO_E_UNKNOWN_ERROR;
	}

	/* Attempt to detect the eyes */
	pEyesClassifier->detectMultiScale( mFaceImage( rgFaceRects[0] ), rgEyeRects, 1.1, 2, 0 | CASCADE_SCALE_IMAGE, Size( 30, 30 ) );
	if ( rgEyeRects.size() < 2 )
	{
		*pRejectReason = OTFBIO_RJR_FF_EYES_CLOSED;
		return OUTFLEDBIO_E_UNKNOWN_ERROR;
	}

	return S_OK;
}

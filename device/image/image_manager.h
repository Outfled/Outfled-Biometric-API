#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <tuple>

#define OTFBIO_RJR_FF_FACE_NOT_FOUND	((ULONGLONG)11)	/* No face was detected from the camera */
#define OTFBIO_RJR_FF_EYES_CLOSED		((ULONGLONG)12)	/* Eyes were closed */
#define OTFBIO_RJR_FF_MULTIPLE_FACES	((ULONGLONG)13)	/* Multiple faces were detected */

class CImageManager
{
private:
	IMFActivate		**m_ppDeviceArray;
	UINT32			m_nDeviceCount;
	IMFActivate		*m_pSelectedDevice;
	IMFMediaSource	*m_pCamSource;
	IMFSourceReader *m_pSourceReader;

	LPVOID m_pFaceClassifier;
	LPVOID m_pEyesClassifier;

	LPVOID m_pTrainerFunction;
	LPVOID m_pClassifierFunction;

	std::vector<LPSTR> m_rgEnrollmentImages;

public:
	CImageManager();

	~CImageManager();

	HRESULT Refresh();

	DWORD GetCameraCount() const;

	BOOL EnumerateCameras( DWORD dwIndex, LPWSTR *ppszCameraName );

	HRESULT SelectDevice( DWORD dwIndex );

	HRESULT StartEnrollment();

	HRESULT CaptureEnrollmentSample( ULONGLONG *pRejectReason );

	HRESULT CommitEnrollment( LPCWSTR lpszName );

	HRESULT CancelEnrollment();

	HRESULT IdentifySample( ULONGLONG *pRejectReason );

private:
	VOID ClearDeviceArray();

	HRESULT SetCameraMediaType();

	HANDLE CreateTempImageFile( LPSTR lpszFilePath );

	HRESULT TakeCameraSnapshot( HANDLE hOutputFile );

	HRESULT ValidateImageSample( LPCSTR lpszFilePath, ULONGLONG *pRejectReason );
};
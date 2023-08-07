#include "pch.h"
#include "audio_manager.h"

CAudioManager::CAudioManager()
{
	m_ppDeviceArray = nullptr;
	m_nDeviceCount = 0;
	m_pSelectedDevice = nullptr;
}

CAudioManager::~CAudioManager()
{
	ClearDeviceArray();
}

HRESULT CAudioManager::Refresh()
{
	IMFAttributes	*pAudioAttributes;
	HRESULT			hResult;

	ClearDeviceArray();

	//
	// Create microphone attributes 
	//
	hResult = MFCreateAttributes( &pAudioAttributes, 1 );
	if ( hResult != S_OK ) {
		return hResult;
	}

	pAudioAttributes->SetGUID( MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_AUDCAP_GUID );

	/* Get array of microphone devices */
	hResult = MFEnumDeviceSources( pAudioAttributes, &m_ppDeviceArray, &m_nDeviceCount );
	pAudioAttributes->Release();

	return hResult;
}

BOOL CAudioManager::EnumerateMicrophones( DWORD dwIndex, LPWSTR *ppszMicrophone )
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
		if ( FAILED( hResult ) ) {
			return FALSE;
		}

		*ppszMicrophone = new WCHAR[cchFriendlyName + 1];
		wcscpy_s( *ppszMicrophone, cchFriendlyName + 1, lpszFriendlyName );

		CoTaskMemFree( lpszFriendlyName );
		return TRUE;
	}

	return FALSE;
}

DWORD CAudioManager::GetMicrophoneCount() const
{
	return m_nDeviceCount;
}

VOID CAudioManager::ClearDeviceArray()
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

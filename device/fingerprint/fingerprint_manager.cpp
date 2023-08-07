#include "pch.h"
#include "fingerprint_manager.h"
#include <WinBio.h>

#pragma comment (lib, "WinBio.lib")


CFingerprintManager::CFingerprintManager()
{
	m_pUnitsArray	= nullptr;
	m_nUnitCount	= 0;
	m_hSession		= 0;
	m_nSelectedUnit = 0;

	WinBioOpenSession( WINBIO_TYPE_FINGERPRINT,
		WINBIO_POOL_SYSTEM,
		WINBIO_FLAG_DEFAULT,
		NULL,
		0,
		NULL,
		&m_hSession
	);
}

CFingerprintManager::~CFingerprintManager()
{
	if ( m_pUnitsArray ) {
		WinBioFree( m_pUnitsArray );
	}

	WinBioCloseSession( m_hSession );
}

HRESULT CFingerprintManager::Refresh()
{
	if ( m_pUnitsArray )
	{
		WinBioFree( m_pUnitsArray );
		m_pUnitsArray = nullptr;
	}

	/* Get fingerprint units */
	return WinBioEnumBiometricUnits( WINBIO_TYPE_FINGERPRINT, (WINBIO_UNIT_SCHEMA **)&m_pUnitsArray, &m_nUnitCount );
}

SIZE_T CFingerprintManager::GetUnitCount() const
{
	return m_nUnitCount;
}

BOOL CFingerprintManager::EnumerateUnits( DWORD dwIndex, LPWSTR *ppszUnitName )
{
	if ( m_pUnitsArray && dwIndex < m_nUnitCount )
	{
		LPCWSTR lpszDescription = ( (WINBIO_UNIT_SCHEMA *)m_pUnitsArray )[dwIndex].Description;

		*ppszUnitName = new WCHAR[wcslen( lpszDescription ) + 1];
		wcscpy_s( *ppszUnitName, wcslen( lpszDescription ) + 1, lpszDescription );

		return TRUE;
	}

	return FALSE;
}

HRESULT CFingerprintManager::SelectUnit( DWORD dwIndex )
{
	if ( dwIndex >= m_nUnitCount ) {
		return HRESULT_FROM_WIN32( ERROR_INDEX_OUT_OF_BOUNDS );
	}

	m_nSelectedUnit = ( (WINBIO_UNIT_SCHEMA *)m_pUnitsArray )[dwIndex].UnitId;
	return S_OK;
}

HRESULT CFingerprintManager::StartEnrollment( UCHAR uSubType )
{
	return WinBioEnrollBegin( m_hSession, uSubType, m_nSelectedUnit );
}

HRESULT CFingerprintManager::CaptureEnrollmentSample( ULONGLONG *pRejectReason )
{
	WINBIO_REJECT_DETAIL rejectDetail;
	HRESULT					hResult;

	hResult = WinBioEnrollCapture( m_hSession, &rejectDetail );
	*pRejectReason = (ULONGLONG)rejectDetail;

	return hResult;
}

HRESULT CFingerprintManager::CancelEnrollment()
{
	return WinBioEnrollDiscard( m_hSession );
}

HRESULT CFingerprintManager::CommitEnrollment()
{
	BOOLEAN			bNewTemplate;
	WINBIO_IDENTITY wIdentity;

	return WinBioEnrollCommit( m_hSession, &wIdentity, &bNewTemplate );
}

HRESULT CFingerprintManager::IdentifySample( ULONGLONG *pRejectReason )
{
	WINBIO_UNIT_ID				wScannerId;
	WINBIO_IDENTITY				wIdentity;
	WINBIO_REJECT_DETAIL		wRejectDetail;
	WINBIO_BIOMETRIC_SUBTYPE	wSubType;
	HRESULT						hResult;

	ZeroMemory( &wIdentity, sizeof( WINBIO_IDENTITY ) );

	/* Identify the fingerprint scan */
	hResult = WinBioIdentify( m_hSession, &wScannerId, &wIdentity, &wSubType, &wRejectDetail );
	*pRejectReason = (ULONGLONG)wRejectDetail;

	return hResult;
}
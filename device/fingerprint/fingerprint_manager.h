#pragma once

typedef enum _OTFBIO_FINGERPRINT_FACTOR_
{
	OTFBIO_FP_FACTOR_RH_THUMB	= (ULONG)1,
	OTFBIO_FP_FACTOR_RH_INDEX	= (ULONG)2,
	OTFBIO_FP_FACTOR_RH_MIDDLE	= (ULONG)3,
	OTFBIO_FP_FACTOR_RH_RING	= (ULONG)4,
	OTFBIO_FP_FACTOR_RH_PINKY	= (ULONG)5,
	OTFBIO_FP_FACTOR_LH_THUMB	= (ULONG)6,
	OTFBIO_FP_FACTOR_LH_INDEX	= (ULONG)7,
	OTFBIO_FP_FACTOR_LH_MIDDLE	= (ULONG)8,
	OTFBIO_FP_FACTOR_LH_RING	= (ULONG)9,
	OTFBIO_FP_FACTOR_LH_PINKY	= (ULONG)10
} OTFBIO_FINGERPRINT_FACTOR, *LPOTFBIO_FINGERPRINT_FACTOR;


//
// Sample Reject Reasons
//
#define OTFBIO_RJR_FP_TOO_HIGH		((ULONGLONG)1)	/* Fingerprint too high */
#define OTFBIO_RJR_FP_TOO_LOW		((ULONGLONG)2)	/* Fingerprint too low */
#define OTFBIO_RJR_FP_TOO_LEFT		((ULONGLONG)3)	/* Fingerprint too much to the left */
#define OTFBIO_RJR_FP_TOO_RIGHT		((ULONGLONG)4)	/* Fingerprint too much to the right */
#define OTFBIO_RJR_FP_TOO_FAST		((ULONGLONG)5)	/* Fingerprint was swiped too quicky */
#define OTFBIO_RJR_FP_TOO_SLOW		((ULONGLONG)6)	/* Fingerprint was swiped too slowly */
#define OTFBIO_RJR_FP_TOO_SKEWED	((ULONGLONG)8)	/* The finger did not pass straight across the sensor */
#define OTFBIO_RJR_FP_TOO_SHORT		((ULONGLONG)9)	/* Not enough of the finger was scanned */
#define OTFBIO_RJR_FP_MERGE_FAIL	((ULONGLONG)10)	/* The fingerprint captures could not be combined */

class CFingerprintManager
{
private:
	LPVOID	m_pUnitsArray;
	SIZE_T	m_nUnitCount;
	ULONG	m_hSession;
	ULONG	m_nSelectedUnit;

public:
	CFingerprintManager();

	~CFingerprintManager();

	HRESULT Refresh();

	SIZE_T GetUnitCount() const;

	BOOL EnumerateUnits( DWORD dwIndex, LPWSTR *ppszUnitName );

	HRESULT SelectUnit( DWORD dwIndex );

	HRESULT StartEnrollment( UCHAR uSubType );

	HRESULT CaptureEnrollmentSample( ULONGLONG *pRejectReason );

	HRESULT CancelEnrollment();

	HRESULT CommitEnrollment();

	HRESULT IdentifySample( ULONGLONG *pRejectReason );
};


#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>


//
// Audio Manager Class for Voice Identification
// Currently not supported
//

class CAudioManager
{
private:
	IMFActivate **m_ppDeviceArray;
	UINT32		m_nDeviceCount;
	IMFActivate *m_pSelectedDevice;

public:
	CAudioManager();

	~CAudioManager();

	HRESULT Refresh();

	DWORD GetMicrophoneCount() const;

	BOOL EnumerateMicrophones( DWORD dwIndex, LPWSTR *ppszMicrophone );

private:
	VOID ClearDeviceArray();

};
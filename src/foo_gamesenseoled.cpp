
#include "../lib/foobar2000_sdk/foobar2000/SDK/foobar2000.h"
#include <WinInet.h>
#include <atlbase.h>
#include <sstream>

DECLARE_COMPONENT_VERSION("foo_gamesenseoled","1","Show track information on your GameDAC\n");

#define QUERY_TITLE "TITLE"
#define QUERY_ARTIST "ARTIST"

#define CONFIG_PATH "C:\\ProgramData\\SteelSeries\\SteelSeries Engine 3\\coreProps.json"
#define CONTENT_HEADER "Content-Type: application/json"
#define TITLE_NAME "FooBarOled"
#define AUTHOR "Weespin"

enum PlaybackType
{
	PLAYING,
	PAUSED,
	STOPPED
};
class GameSenseCommunicator
{
	public:
	void Init()
	{
		 m_hInternet = InternetOpenA("foo_gamesenseoled", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
		 m_hConnect = InternetConnectA(m_hInternet, "127.0.0.1", CommunicatorPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1);
	}
	void SendPost(const char* pszEndpoint, const char* pszData)
	{
		HINTERNET hRequest = HttpOpenRequestA(m_hConnect, "POST", pszEndpoint, NULL, NULL, NULL, 0, 1);
		bool bSent = HttpSendRequestA(hRequest, CONTENT_HEADER, strlen(CONTENT_HEADER), (char*)pszData, strlen(pszData));
		InternetCloseHandle(hRequest);
	}
	void RegisterGame()
	{
		SendPost("game_metadata","{\"game\":\"FOO_GAMESENSE_OLED\",\"game_display_name\":\"Foobar2000 track information\",\"developer\":\"Weespin\"}\)");
	}
	void BindEvent()
	{
		SendPost("bind_game_event", "{\"game\":\"FOO_GAMESENSE_OLED\",\"event\":\"MEDIA_PLAYBACK\",\"handlers\":[{\"device-type\":\"screened-128x52\",\"zone\":\"one\",\"mode\":\"screen\",\"datas\":[{\"lines\":[{\"has-text\":true,\"context-frame-key\":\"track-name\"},{\"has-text\":true,\"context-frame-key\":\"artist-name\"},{\"has-progress-bar\":true}]}]}]}");
		SendPost("bind_game_event", "{\"game\":\"FOO_GAMESENSE_OLED\",\"event\":\"MEDIA_PAUSED\",\"handlers\":[{\"device-type\":\"screened-128x52\",\"zone\":\"one\",\"mode\":\"screen\",\"datas\":[{\"icon-id\":25,\"lines\":[{\"has-text\":true,\"context-frame-key\":\"track-name\"},{\"has-text\":true,\"context-frame-key\":\"artist-name\"}]}]}]}");
		SendPost("bind_game_event", "{\"game\":\"FOO_GAMESENSE_OLED\",\"event\":\"MEDIA_CREDITS\",\"handlers\":[{\"device-type\":\"screened-128x52\",\"zone\":\"one\",\"mode\":\"screen\",\"datas\":[{\"icon-id\":40,\"lines\":[{\"has-text\":true,\"context-frame-key\":\"track-name\"},{\"has-text\":true,\"context-frame-key\":\"artist-name\"}]}]}]}");

	}
	void SendCreditsEvent()
	{
		std::stringstream stream;
		stream <<  "{\"game\":\"FOO_GAMESENSE_OLED\",\"event\":\"MEDIA_CREDITS\",\"data\":{\"value\":0,\"frame\":{\"track-name\":\"" << TITLE_NAME << "\",\"artist-name\":\"" << "by "<< AUTHOR << "\"}}}";
		std::string result = stream.str();
		SendPost("game_event", result.c_str());
	}
	void SendPlayingEvent(const char* pszTrackName,const char* pszArtistName, unsigned long nValue)
	{
		std::stringstream stream;
		stream << "{\"game\":\"FOO_GAMESENSE_OLED\",\"event\":\"MEDIA_PLAYBACK\",\"data\":{\"value\":" << nValue << ".0" << rand() % 49 << ",\"frame\":{\"track-name\":\"" << pszTrackName << "\",\"artist-name\":\"" << pszArtistName << "\"}}}";
		std::string result = stream.str();
		SendPost("game_event", result.c_str());
		//LOG(INFO,"Playing: %s %s Len: %i",pszTrackName,pszArtistName,nValue);
	}
	void SendPausedEvent(const char* pszTrackName, const char* pszArtistName)
	{
		std::stringstream stream;
		stream << "{\"game\":\"FOO_GAMESENSE_OLED\",\"event\":\"MEDIA_PAUSED\",\"data\":{\"value\":" << rand() % 100 << ",\"frame\":{\"track-name\":\"" << pszTrackName << "\",\"artist-name\":\"" << pszArtistName << "\"}}}";
		std::string result = stream.str();
		SendPost("game_event", result.c_str());
		//LOG(INFO, "Paused: %s %s", pszTrackName, pszArtistName);
	}
	void SetCommunicationPort(unsigned short nPort)
	{
		CommunicatorPort = nPort;
	}

	unsigned short CommunicatorPort;
	HINTERNET m_hInternet;
	HINTERNET m_hConnect;
};

class GameSenseOled
{
public:
	GameSenseOled()
	{
		m_pSection = new CRITICAL_SECTION;
		InitializeCriticalSection(m_pSection);
		//Find gamesense port
		HANDLE hFile = CreateFileA(CONFIG_PATH, GENERIC_READ, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		
		unsigned long nFileSize = GetFileSize(hFile, NULL);
		char* szContent = new char[nFileSize];
		unsigned long nBytesRead = 0;
		bool bResult = ReadFile(hFile,(void*)szContent, nFileSize,&nBytesRead,NULL);
		CloseHandle(hFile);
		char* pszDecryptedPort = strstr(szContent, "127.0.0.1:");
		if(pszDecryptedPort)
		{
			pszDecryptedPort += sizeof("127.0.0.1");
		}
		m_Communicator.SetCommunicationPort(atol(pszDecryptedPort));
		delete[] szContent;
		m_Communicator.Init();
	}
	~GameSenseOled()
	{
		DeleteCriticalSection(m_pSection);
		delete m_pSection;
	}
	void Poll()
	{
		if(!m_bIsInited)
		{
			m_Communicator.RegisterGame();
			m_Communicator.BindEvent();
			m_bIsInited = true;
			
			m_Communicator.SendCreditsEvent();
			//Register & bind events
		}
		EnterCriticalSection(m_pSection);
		if(m_PlayBackType!=STOPPED)
		{
			if(m_PlayBackType == PAUSED)
			{
				m_Communicator.SendPausedEvent(m_CurrentTrackName.c_str(), m_CurrentArtistName.c_str());
				if (m_CurrentTrackName.size() > 15)
				{
					std::rotate(m_CurrentTrackName.begin(), m_CurrentTrackName.begin() + 1, m_CurrentTrackName.end());
				}
				if (m_CurrentArtistName.size() > 15)
				{
					std::rotate(m_CurrentArtistName.begin(), m_CurrentArtistName.begin() + 1, m_CurrentArtistName.end());
				}
			}
			else
			{
				
				m_Communicator.SendPlayingEvent(m_CurrentTrackName.c_str(),m_CurrentArtistName.c_str(), (unsigned long) 100/(m_nTrackDuration/m_nCurrentTrackPosition));
				if (m_CurrentTrackName.size() > 20)
				{
					std::rotate(m_CurrentTrackName.begin(), m_CurrentTrackName.begin() + 1, m_CurrentTrackName.end());
				}
				if (m_CurrentArtistName.size() > 20)
				{
					std::rotate(m_CurrentArtistName.begin(), m_CurrentArtistName.begin() + 1, m_CurrentArtistName.end());
				}
			}
			
			
		}
		LeaveCriticalSection(m_pSection);
	}
	void SetTrackName(const char* pszTrackName)
	{
		if(!pszTrackName)
		{
			return;
		}
		EnterCriticalSection(m_pSection);
		
		m_OriginalTrackName = pszTrackName;
		m_OriginalTrackName += "   ";
		m_CurrentTrackName = m_OriginalTrackName;
		LeaveCriticalSection(m_pSection);
	}
	void SetArtistName(const char* pszArtistName)
	{
		if (!pszArtistName)
		{
			return;
		}
		EnterCriticalSection(m_pSection);
		m_OriginalArtistName = pszArtistName;
		m_OriginalArtistName += "   ";
		m_CurrentArtistName = m_OriginalArtistName;

		LeaveCriticalSection(m_pSection);

	}
	void SetTrackDuration(double nDuration)
	{
		m_nTrackDuration = nDuration;
	}
	void SetTrackPosition(double nPosition)
	{
		m_nCurrentTrackPosition = nPosition;
	}
	void SetPlaybackType(PlaybackType type)
	{
		m_PlayBackType = type;
		m_CurrentArtistName = m_OriginalArtistName;
		m_CurrentTrackName = m_OriginalTrackName;
	}
	
private:
	
	std::string				m_CurrentTrackName;
	std::string				m_CurrentArtistName;
	std::string				m_OriginalTrackName;
	std::string				m_OriginalArtistName;
	GameSenseCommunicator	m_Communicator;
	LPCRITICAL_SECTION		m_pSection;
	double					m_nTrackDuration;
	double					m_nCurrentTrackPosition;
	PlaybackType			m_PlayBackType = STOPPED;
	bool					m_bIsInited = false;
};
static GameSenseOled* pOledManager;
void MainThread()
{
	pOledManager = new GameSenseOled();
	while(true)
	{
		pOledManager->Poll();
		Sleep(500);
	}
}


class my_play_callback_static : public play_callback_static
{
protected:
	my_play_callback_static() : m_target(SIZE_MAX), m_counter(0)
	{
		CreateThread(0,0, (LPTHREAD_START_ROUTINE)MainThread,0,0,0);
	}
	~my_play_callback_static() {}

	void on_playback_new_track(metadb_handle_ptr p_track) override
	{
		pOledManager->SetPlaybackType(PLAYING);
		
		pOledManager->SetTrackDuration(p_track->get_length());
		metadb_info_container::ptr container = p_track->get_async_info_ref();
		const file_info& info = container->info();
		pOledManager->SetTrackPosition(-1);
		
		if (info.meta_exists(QUERY_TITLE))
		{
			pOledManager->SetTrackName(info.meta_get(QUERY_TITLE, 0));
		}
		else
		{
			pOledManager->SetArtistName("");
			pOledManager->SetTrackName(pfc::io::path::getFileNameWithoutExtension(p_track->get_path()).c_str());
			return;
		}
		if (info.meta_exists(QUERY_ARTIST))
		{
			pOledManager->SetArtistName(info.meta_get(QUERY_ARTIST, 0));
		}
	}
	titleformat_object::ptr m_script;
	void on_playback_time(double p_time) override
	{
		pOledManager->SetTrackPosition(p_time);
	}
	void on_playback_pause(bool p_state) override
	{
		pOledManager->SetPlaybackType(p_state ? PAUSED : PLAYING);
	}
	void on_playback_stop(play_control::t_stop_reason p_reason) override
	{
		pOledManager->SetPlaybackType(STOPPED);

	}
	void on_playback_starting(play_control::t_track_command p_command, bool p_paused) override
	{
		pOledManager->SetPlaybackType(PLAYING);

	}
	void on_playback_seek(double p_time) override{}
	void on_playback_edited(metadb_handle_ptr p_track) override {}
	void on_playback_dynamic_info(const file_info& p_info) override{}
	void on_playback_dynamic_info_track(const file_info& p_info) override{}
	void on_volume_change(float p_new_val) override {}

	unsigned int get_flags() override
	{
		return flag_on_playback_all;
	}

private:
	enum listen_type
	{
		playing_now,
		single
	};

	std::string m_Artist;
	std::string m_Title;
	double nTrackLen = 0;
	t_size m_counter, m_target;
};
static play_callback_static_factory_t<my_play_callback_static> g_my_play_callback_static;

//	Soundtrack.h
//
//	Classes and methods for playing music
//	Copyright (c) 2013 by Kronosaur Productions, LLC. All Rights Reserved.

#pragma once

class CMCIMixer
	{
	public:
		CMCIMixer (int iChannels = 2);
		~CMCIMixer (void);

		void AbortAllRequests (void);
		void FadeAtPos (int iPos);
		void FadeNow (void);
		int GetCurrentPlayLength (void) const;
		int GetCurrentPlayPos (void) const;
		void GetDebugInfo (TArray<CString> *retLines) const;
		bool Play (CSoundType *pTrack, int iPos = 0);
		bool PlayFadeIn (CSoundType *pTrack, int iPos = 0);
		void SetPlayPaused (bool bPlay);
		void SetVolume (int iVolume);
		void Shutdown (void);
		void Stop (void);
		void TogglePausePlay (void);

	private:
		enum EChannelState
			{
			stateNone,
			statePlaying,
			};

		struct SChannel
			{
			HWND hMCI;
			EChannelState iState;
			};

		enum ERequestType
			{
			typeNone,
			typePlay,
			typeStop,
			typePlayPause,
			typeWaitForPos,
			typeFadeIn,
			typeFadeOut,
			typeSetPaused,
			typeSetUnpaused,
			};

		struct SRequest
			{
			ERequestType iType;
			CSoundType *pTrack;
			int iPos;
			};

		void CreateParentWindow (void);
		void EnqueueRequest (ERequestType iType, CSoundType *pTrack = NULL, int iPos = 0);
		bool FindChannel (HWND hMCI, SChannel **retpChannel = NULL);
		CString GetRequestDesc (const SRequest &Request) const;
		void LogError (HWND hMCI, const CString &sState, const CString &sFilespec = NULL_STR);
		LONG OnNotifyMode (HWND hWnd, int iMode);
		LONG OnNotifyPos (HWND hWnd, int iPos);
		void ProcessFadeIn (const SRequest &Request);
		void ProcessFadeOut (const SRequest &Request);
		void ProcessPlay (const SRequest &Request);
		void ProcessPlayPause (const SRequest &Request);
		bool ProcessRequest (void);
		void ProcessSetPlayPaused (const SRequest &Request);
		void ProcessStop (const SRequest &Request, bool bNoNotify = false);
		void ProcessWaitForPos (const SRequest &Request);
		bool Wait (DWORD dwTimeout);

		static LONG APIENTRY ParentWndProc (HWND hWnd, UINT message, UINT wParam, LONG lParam);
		static DWORD WINAPI ProcessingThread (LPVOID pData);

		HWND m_hParent;
		int m_iDefaultVolume;				//	Default volume
		TArray<SChannel> m_Channels;
		int m_iCurChannel;
		CSoundType *m_pNowPlaying;			//	Currently playing

		//	Synchronization

		mutable CCriticalSection m_cs;
		TQueue<SRequest> m_Request;			//	Queue of requests
		HANDLE m_hProcessingThread;			//	Processing thread handle
		HANDLE m_hQuitEvent;				//	Tell thread to quit
		HANDLE m_hWorkEvent;				//	Tell thread to work
		HANDLE m_hAbortEvent;				//	Tell thread to stop
		bool m_bNoStopNotify;

#ifdef DEBUG_SOUNDTRACK_STATE
		SRequest m_CurRequest;				//	Request being processed
#endif
	};

class CSoundtrackManager
	{
	public:
		enum EGameStates
			{
			stateNone,

			stateProgramLoad,				//	Loading the program
			stateProgramIntro,				//	In intro screen
			stateGamePrologue,				//	Creating a new game
			stateGameTravel,				//	Normal play; travel music
			stateGameCombat,				//	Combat music
			stateGameEpitaph,				//	Game over
			stateProgramQuit,				//	Program is closing
			};

		CSoundtrackManager (void);
		~CSoundtrackManager (void);

		CSoundType *GetCurrentTrack (int *retiPos = NULL) const;
		void NextTrack (void);
		void NotifyEndCombat (void);
		void NotifyEnterSystem (CTopologyNode *pNode = NULL, bool bFirstTime = true);
		inline void NotifyGameStart (void) { Reinit(); }
		void NotifyStartCombat (void);
		void NotifyStartCombatMission (void);
		void NotifyTrackDone (void);
		void NotifyTrackPlaying (CSoundType *pTrack);
		void NotifyUndocked (void);
		void NotifyUpdatePlayPos (int iPos);
		void PaintDebugInfo (CG32bitImage &Dest, const RECT &rcScreen);
		void SetGameState (EGameStates iNewState);
		void SetGameState (EGameStates iNewState, CSoundType *pTrack);
		void SetMusicEnabled (bool bEnabled = true);
		void SetPlayPaused (bool bPlay = true);
		void SetVolume (int iVolume);
		void TogglePlayPaused (void);

	private:
		CSoundType *CalcGameTrackToPlay (CTopologyNode *pNode, const CString &sRequiredAttrib) const;
		CSoundType *CalcRandomTrackToPlay (void) const;
		CSoundType *CalcTrackToPlay (CTopologyNode *pNode, EGameStates iNewState) const;
		int GetLastPlayedRank (DWORD dwUNID) const;
		bool IsPlayingCombatTrack (void) const;
		bool InTransition (void) const;
		void Play (CSoundType *pTrack);
		void Reinit (void);
		void TransitionTo (CSoundType *pTrack, int iPos, bool bFadeIn = false);
		void TransitionToCombat (void);
		void TransitionToTravel (void);

		CMCIMixer m_Mixer;					//	Music mixer
		bool m_bEnabled;					//	Music is enabled
		EGameStates m_iGameState;			//	Current soundtrack state
		CSoundType *m_pNowPlaying;			//	What we've scheduled to play
		CSoundType *m_pLastTravel;			//	Travel music track interrupted by combat

		TQueue<DWORD> m_LastPlayed;			//	UNID of tracks played.
		bool m_bSystemTrackPlayed;			//	systemSoundtrack already played in system.
		bool m_bStartCombatWhenUndocked;	//	If TRUE, we play combat music when we undock
		DWORD m_dwTransition;				//	Tick on which we started a transition
		DWORD m_dwStartedCombat;			//	Millisecond on which we started combat
		DWORD m_dwStartedTravel;			//	Millisecond on which we started travel mode

		CSoundType *m_pIntroTrack;			//	Track to play for intro.
	};

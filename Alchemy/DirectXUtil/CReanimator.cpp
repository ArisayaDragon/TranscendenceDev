//	CReanimator.cpp
//
//	CReanimator class
//	Copyright (c) 2012 by Kronosaur Productions, LLC. All Rights Reserved.

#include "PreComp.h"
#include "Reanimator.h"

CG16bitFont *CReanimator::m_pDefaultFont = NULL;

CReanimator::CReanimator (void) :
		m_dwNextID(1),
		m_iPlaySpeed(1),
		m_iFastPlayCounter(-1),
		m_ToDest(xformIdentity),
        m_ToLocal(xformIdentity),
		m_pInputFocus(NULL),
		m_pMouseCapture(NULL),
		m_pHover(NULL)

//	CReanimator constructor

	{
	}

CReanimator::~CReanimator (void)

//	CReanimator destructor

	{
	int i;

	for (i = 0; i < m_Library.GetCount(); i++)
		delete m_Library[i].pAni;
	}

DWORD CReanimator::AddPerformance (IAnimatron *pAni, const CString &sID)

//	AddPerformance
//
//	Adds a performance and returns its ID. The Reanimator takes
//	ownership of the object.

	{
	ASSERT(pAni);

	SPerformance *pNewPerf = m_Library.Insert();
	pNewPerf->dwID = m_dwNextID++;
	pNewPerf->sID = sID;
	pNewPerf->pAni = pAni;
	pNewPerf->iFrame = -1;
	pNewPerf->iDuration = 0;
	pNewPerf->bDeleteWhenDone = false;

	if (!sID.IsBlank())
		pAni->SetID(sID);

	return pNewPerf->dwID;
	}

void CReanimator::DeleteAnimatron (IAnimatron *pAni)

//	DeleteAnimatron
//
//	Deletes the given animatron

	{
	if (m_pInputFocus && pAni->FindElement(m_pInputFocus))
		SetInputFocus(NULL);

	if (m_pMouseCapture && pAni->FindElement(m_pMouseCapture))
		m_pMouseCapture = NULL;

	if (m_pHover && pAni->FindElement(m_pHover))
		m_pHover = NULL;

	delete pAni;
	}

void CReanimator::DeleteElement (const CString &sID)

//	DeleteElement
//
//	Deletes the given element by ID

	{
	int i;

	//	Find the element that we're about to delete

	IAnimatron *pToDelete = NULL;
	for (i = 0; i < m_Library.GetCount(); i++)
		if (m_Library[i].pAni->FindElement(sID, &pToDelete))
			break;

	//	If the element is not found, then not need to delete it.

	if (pToDelete == NULL)
		return;

	//	See if deleting this element will affect the focus or capture

	if (m_pInputFocus && pToDelete->FindElement(m_pInputFocus))
		SetInputFocus(NULL);

	if (m_pMouseCapture && pToDelete->FindElement(m_pMouseCapture))
		m_pMouseCapture = NULL;

	if (m_pHover && pToDelete->FindElement(m_pHover))
		m_pHover = NULL;

	//	Delete the element

	for (i = 0; i < m_Library.GetCount(); i++)
		{
		if (strEquals(m_Library[i].pAni->GetID(), sID))
			{
			delete m_Library[i].pAni;
			m_Library.Delete(i);
			break;
			}
		else if (m_Library[i].pAni->DeleteElement(sID))
			break;
		}
	}

void CReanimator::DeletePerformance (DWORD dwID)

//	DeletePerformance
//
//	Remove the given performance

	{
	int i;

	for (i = 0; i < m_Library.GetCount(); i++)
		if (m_Library[i].dwID == dwID)
			{
			DeleteAnimatron(m_Library[i].pAni);
			m_Library.Delete(i);
			break;
			}
	}

CReanimator::SPerformance *CReanimator::Find (DWORD dwID)

//	Find
//
//	Finds the performance by ID

	{
	int i;

	for (i = 0; i < m_Library.GetCount(); i++)
		if (m_Library[i].dwID == dwID)
			return &m_Library[i];

	return NULL;
	}

CReanimator::SPerformance *CReanimator::Find (const CString &sID)

//	Find
//
//	Finds the performance by sID

	{
	int i;

	for (i = 0; i < m_Library.GetCount(); i++)
		if (strEquals(m_Library[i].sID, sID))
			return &m_Library[i];

	return NULL;
	}

const CG16bitFont &CReanimator::GetDefaultFont (void)

//	GetDefaultFont
//
//	Returns the default font

	{
	if (m_pDefaultFont == NULL)
		{
		m_pDefaultFont = new CG16bitFont;
		m_pDefaultFont->Create(CONSTLIT("Tahoma"), 16, true);
		}

	return *m_pDefaultFont;
	}

IAnimatron *CReanimator::GetElement (const CString &sID) const

//	GetElement
//
//	Finds the given element by ID

	{
	int i;

	for (i = 0; i < m_Library.GetCount(); i++)
		{
		SPerformance *pPerf = &m_Library[i];
		if (pPerf->iFrame != -1)
			{
			IAnimatron *pElement;
			if (pPerf->pAni->FindElement(sID, &pElement))
				return pElement;
			}
		}

	return NULL;
	}

bool CReanimator::GetFocusElements (TArray<IAnimatron *> *retList, int *retiCurrent)

//	GetFocusElements
//
//	Get the list of focus elements

	{
	int i;

	for (i = 0; i < m_Library.GetCount(); i++)
		{
		SPerformance *pPerf = &m_Library[i];
		if (pPerf->iFrame != -1)
			pPerf->pAni->GetFocusElements(retList);
		}

	//	Look for the current focus

	if (retiCurrent)
		{
		*retiCurrent = -1;
		for (i = 0; i < retList->GetCount(); i++)
			if (retList->GetAt(i) == m_pInputFocus)
				{
				*retiCurrent = i;
				break;
				}
		}

	//	Return TRUE if the list has elements

	return (retList->GetCount() > 0);
	}

bool CReanimator::HandleChar (char chChar, DWORD dwKeyData)

//	HandleChar
//
//	Input character

	{
	switch (chChar)
		{
		case VK_TAB:
			if (uiIsShiftDown())
				SetInputFocusPrev();
			else
				SetInputFocusNext();
			return true;

		default:
			if (m_pInputFocus == NULL)
				return false;

			return m_pInputFocus->HandleChar(chChar, dwKeyData);
		}
	}

bool CReanimator::HandleKeyDown (int iVirtKey, DWORD dwKeyData)

//	HandleKeyDown
//
//	Input key

	{
	if (m_pInputFocus == NULL)
		return false;

	return m_pInputFocus->HandleKeyDown(iVirtKey, dwKeyData);
	}

bool CReanimator::HandleKeyUp (int iVirtKey, DWORD dwKeyData)

//	HandleKeyUp
//
//	Input key

	{
	if (m_pInputFocus == NULL)
		return false;

	return m_pInputFocus->HandleKeyUp(iVirtKey, dwKeyData);
	}

bool CReanimator::HandleLButtonDblClick (int x, int y, DWORD dwFlags, bool *retbCapture)

//	HandleLButtonDblClick
//
//	Left button double click

	{
    SAniHitTestResult Result(x, y);
	if (HitTest(Result))
		{
		//	If we haven't sent a MouseEnter event, then do it now.

		if (Result.pHit != m_pHover)
			{
			m_pHover = Result.pHit;
			m_pHover->HandleMouseEnter();
			}

		bool bFocus;
		Result.pHit->HandleLButtonDblClick(Result.xLocal, Result.yLocal, dwFlags, retbCapture, &bFocus);
		if (*retbCapture)
			m_pMouseCapture = Result.pHit;

		if (bFocus)
			SetInputFocus(Result.pHit);

		return true;
		}

	return false;
	}

bool CReanimator::HandleLButtonDown (int x, int y, DWORD dwFlags, bool *retbCapture)

//	HandleLButtonDown
//
//	Left button down

	{
    SAniHitTestResult Result(x, y);
	if (HitTest(Result))
		{
		//	If we haven't sent a MouseEnter event, then do it now.

		if (Result.pHit != m_pHover)
			{
			m_pHover = Result.pHit;
			m_pHover->HandleMouseEnter();
			}

		bool bFocus;
		Result.pHit->HandleLButtonDown(Result.xLocal, Result.yLocal, dwFlags, retbCapture, &bFocus);
		if (*retbCapture)
			m_pMouseCapture = Result.pHit;

		if (bFocus)
			SetInputFocus(Result.pHit);

		return true;
		}

	return false;
	}

bool CReanimator::HandleLButtonUp (int x, int y, DWORD dwFlags)

//	HandleLButtonUp
//
//	Left button up

	{
    SAniHitTestResult Result(x, y);
	HitTest(Result);

	if (m_pMouseCapture)
		{
		//	If we're not over the element that captured the mouse then we need
		//	to send a mouse enter event

		if (Result.pHit && Result.pHit != m_pMouseCapture)
			{
			m_pHover = Result.pHit;
			m_pHover->HandleMouseEnter();
			}

		//	Keep the element that we hit on the stack because the event might
		//	destroy everything.

		Result.pHit = m_pMouseCapture;
		m_pMouseCapture = NULL;

		Result.pHit->HandleLButtonUp(Result.xLocal, Result.yLocal, dwFlags);
		return true;
		}
	else if (Result.pHit)
		{
		Result.pHit->HandleLButtonUp(Result.xLocal, Result.yLocal, dwFlags);
		return true;
		}
	else
		return false;
	}

bool CReanimator::HandleMouseMove (int x, int y, DWORD dwFlags)

//	HandleMouseMove
//
//	Mouse moves

	{
	bool bHandled = false;
    SAniHitTestResult Result(x, y, m_pMouseCapture);

    //  If we've captured the mouse, we always get back local coordinates,
    //  but Result.pHit is non-NULL only if we are inside the animatron that
    //  captured the mouse. Note, however, that HitTest always returns TRUE
    //  in either case (because Result is valid).

	HitTest(Result);

	if (m_pMouseCapture)
		{
		m_pMouseCapture->HandleMouseMove(Result.xLocal, Result.yLocal, dwFlags);
		bHandled = true;
		}
	else if (Result.pHit)
		{
		Result.pHit->HandleMouseMove(x, y, dwFlags);
		bHandled = true;
		}

	//	Send enter/leave messages

	if (Result.pHit != m_pHover)
		{
		if (m_pHover)
			m_pHover->HandleMouseLeave();

		m_pHover = Result.pHit;

		if (m_pHover)
			m_pHover->HandleMouseEnter();
		}

	return bHandled;
	}

bool CReanimator::HandleMouseWheel (int iDelta, int x, int y, DWORD dwFlags)

//	HandleMouseWheel
//
//	Mouse wheel

	{
	if (m_pInputFocus == NULL)
		return false;

    //  If we're not over a reanimator element, then we don't handle it.

    SAniHitTestResult Result(x, y);
    if (!HitTest(Result))
        return false;

    //  Let the input control handle it.

	return m_pInputFocus->HandleMouseWheel(iDelta, x, y, dwFlags);
	}

bool CReanimator::HandleRButtonDblClick (int x, int y, DWORD dwFlags, bool *retbCapture)

//	HandleRButtonDblClick
//
//	Right button double click

	{
    SAniHitTestResult Result(x, y);
	if (HitTest(Result))
		{
		bool bFocus;
		Result.pHit->HandleRButtonDblClick(Result.xLocal, Result.yLocal, dwFlags, retbCapture, &bFocus);
		if (*retbCapture)
			m_pMouseCapture = Result.pHit;

		if (bFocus)
			SetInputFocus(Result.pHit);

		return true;
		}

	return false;
	}

bool CReanimator::HandleRButtonDown (int x, int y, DWORD dwFlags, bool *retbCapture)

//	HandleRButtonDown
//
//	Right button down

	{
    SAniHitTestResult Result(x, y);
	if (HitTest(Result))
		{
		bool bFocus;
		Result.pHit->HandleRButtonDown(Result.xLocal, Result.yLocal, dwFlags, retbCapture, &bFocus);
		if (*retbCapture)
			m_pMouseCapture = Result.pHit;

		if (bFocus)
			SetInputFocus(Result.pHit);

		return true;
		}

	return false;
	}

bool CReanimator::HandleRButtonUp (int x, int y, DWORD dwFlags)

//	HandleRButtonUp
//
//	Right button up

	{
    SAniHitTestResult Result(x, y);

	if (m_pMouseCapture)
		{
		//	If we're not over the element that captured the mouse then we need
		//	to send a mouse enter event

		if (Result.pHit && Result.pHit != m_pMouseCapture)
			{
			m_pHover = Result.pHit;
			m_pHover->HandleMouseEnter();
			}

		//	Keep the element that we hit on the stack because the event might
		//	destroy everything.

		Result.pHit = m_pMouseCapture;
		m_pMouseCapture = NULL;

		Result.pHit->HandleRButtonUp(Result.xLocal, Result.yLocal, dwFlags);
		return true;
		}
	else if (Result.pHit)
		{
		Result.pHit->HandleRButtonUp(Result.xLocal, Result.yLocal, dwFlags);
		return true;
		}
	else
		return false;
	}

bool CReanimator::HitTest (SAniHitTestResult &Result)

//	HitTest
//
//	Returns an element at the given coordinates that wants mouse events

	{
	int i;

	for (i = 0; i < m_Library.GetCount(); i++)
		{
		SPerformance *pPerf = &m_Library[i];
		if ((pPerf->iFrame != -1)
				&& (pPerf->pAni->HitTest(m_ToLocal, Result)))
			return true;
		}

	return false;
	}

bool CReanimator::IsPerformanceRunning (const CString &sID)

//	IsPerformanceRunning
//
//	Returns TRUE if the given performance is running

	{
	int i;

	for (i = 0; i < m_Library.GetCount(); i++)
		if (strEquals(m_Library[i].sID, sID))
			return (m_Library[i].iFrame != -1);

	return false;
	}

bool CReanimator::IsRunning (void)

//	IsRunning
//
//	Returns TRUE if any performance is running

	{
	int i;

	for (i = 0; i < m_Library.GetCount(); i++)
		if (m_Library[i].iFrame != -1)
			return true;

	return false;
	}

bool CReanimator::PaintFrame (CG32bitImage &Dest)

//	PaintFrame
//
//	Paints the current frame and advances to the next one
//	Returns FALSE if nothing was painted

	{
	int i, j;
	bool bPainted = false;

	//	Initialize context

	SAniPaintCtx Ctx(Dest, m_ToDest, 255, 0);
	SAniUpdateCtx UpdateCtx;

	//	Loop over all performances

	for (i = 0; i < m_Library.GetCount(); i++)
		{
		SPerformance *pPerf = &m_Library[i];
		if (pPerf->iFrame != -1)
			{
			//	Initialize the remainder of the paint context

			Ctx.iFrame = pPerf->iFrame;

			//	Paint

			pPerf->pAni->Paint(Ctx);
			bPainted = true;

			//	Advance frame

			if (m_iPlaySpeed > 0)
				{
				for (j = 0; j < m_iPlaySpeed && (pPerf->iDuration < 0 || pPerf->iFrame < pPerf->iDuration); j++)
					{
					pPerf->iFrame++;
					pPerf->pAni->GoToNextFrame(UpdateCtx, pPerf->iFrame);
					}

				//	Done?

				if (pPerf->iDuration >= 0 && pPerf->iFrame >= pPerf->iDuration)
					{
					pPerf->iFrame = -1;
					if (pPerf->bDeleteWhenDone)
						{
						DeleteAnimatron(m_Library[i].pAni);
						m_Library.Delete(i);
						i--;
						}
					}
				}

			//	Reverse

			else
				{
				pPerf->iFrame = Max(0, pPerf->iFrame + m_iPlaySpeed);
				pPerf->pAni->GoToFrame(pPerf->iFrame);
				}
			}
		}

	//	Turn-off advance/rewind if we have a counter

	if (m_iFastPlayCounter != -1 && --m_iFastPlayCounter == 0)
		{
		m_iPlaySpeed = 1;
		m_iFastPlayCounter = -1;
		}

	//	Fire all events

	for (i = 0; i < UpdateCtx.EventsToFire.GetCount(); i++)
		{
		const SAniEvent &Event = UpdateCtx.EventsToFire[i];
		Event.pListener->AniCommand(NULL_STR, Event.sEvent, Event.sCmd, Event.dwData);
		}

	//	Done

	return bPainted;
	}

void CReanimator::SetInputFocus (IAnimatron *pFocus)

//	SetInputFocus
//
//	Sets the input focus

	{
	if (pFocus == m_pInputFocus)
		return;

	if (m_pInputFocus)
		m_pInputFocus->KillFocus();

	m_pInputFocus = pFocus;

	if (m_pInputFocus)
		m_pInputFocus->SetFocus();
	}

void CReanimator::SetInputFocusNext (void)

//	SetInputFocusNext
//
//	Sets the focus to the next element

	{
	int iFocus;
	TArray<IAnimatron *> Elements;

	if (!GetFocusElements(&Elements, &iFocus))
		{
		SetInputFocus(NULL);
		return;
		}

	//	If we can't find the focus then set it to the first element. Otherwise
	//	we advance to the next element in the list.

	SetInputFocus(Elements[(iFocus == -1 ? 0 : (iFocus + 1) % Elements.GetCount())]);
	}

void CReanimator::SetInputFocusPrev (void)

//	SetInputFocusPrev
//
//	Sets the focus to the previous element

	{
	int iFocus;
	TArray<IAnimatron *> Elements;

	if (!GetFocusElements(&Elements, &iFocus))
		{
		SetInputFocus(NULL);
		return;
		}

	//	If we can't find the focus then set it to the last element. Otherwise
	//	we advance to the prev element in the list.

	SetInputFocus(Elements[(iFocus == -1 ? Elements.GetCount() - 1 : (iFocus + Elements.GetCount() - 1) % Elements.GetCount())]);
	}

void CReanimator::StartPerformance (SPerformance *pPerf, DWORD dwFlags, int *retiDuration)

//	StartPerformance
//
//	Starts the given performance

	{
	if (pPerf == NULL)
		return;

	pPerf->pAni->GoToStart();
	pPerf->iDuration = pPerf->pAni->GetDuration();
	if (pPerf->iDuration == 0)
		return;

	pPerf->iFrame = 0;
	pPerf->bDeleteWhenDone = ((dwFlags & SPR_FLAG_DELETE_WHEN_DONE) ? true : false);

	if (retiDuration)
		*retiDuration = pPerf->iDuration;

	//	If we don't have input focus set then set it now

	if (m_pInputFocus == NULL)
		SetInputFocusNext();
	}

void CReanimator::StopPerformance (const CString &sID)

//	StopPerformance
//
//	Remove the given performance

	{
	int i;

	for (i = 0; i < m_Library.GetCount(); i++)
		if (strEquals(m_Library[i].sID, sID))
			{
			m_Library[i].iFrame = -1;
			if (m_Library[i].bDeleteWhenDone)
				{
				DeleteAnimatron(m_Library[i].pAni);
				m_Library.Delete(i);
				i--;
				}
			}
	}

void CReanimator::StopAll (void)

//	StopAll
//
//	Stops all performances

	{
	int i;

	for (i = 0; i < m_Library.GetCount(); i++)
		if (m_Library[i].iFrame != -1)
			{
			m_Library[i].iFrame = -1;
			if (m_Library[i].bDeleteWhenDone)
				{
				DeleteAnimatron(m_Library[i].pAni);
				m_Library.Delete(i);
				i--;
				}
			}

	//	Reset burst mode

	m_iPlaySpeed = 1;
	m_iFastPlayCounter = -1;
	}


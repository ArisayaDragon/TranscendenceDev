//	CComplexArea.cpp
//
//	CComplexArea class
//	Copyright (c) 2011 by George Moromisato. All Rights Reserved.
//
//	NOTE: All coordinates are cartessian

#include "PreComp.h"

const int MAX_TRIES = 100;

const CG32bitPixel RGB_INCLUDED =				CG32bitPixel(0, 255, 255);
const CG32bitPixel RGB_EXCLUDED =				CG32bitPixel(255, 255, 0);

CComplexArea::CComplexArea (void)

//	CComplexArea constructor

	{
	m_rcBounds.left = 1000000;
	m_rcBounds.top = -1000000;
	m_rcBounds.right = -1000000;
	m_rcBounds.bottom = 1000000;
	}

void CComplexArea::AddCircle (TArray<SCircle> &Array, int x, int y, int iRadius)

//	AddCircle
//
//	Adds a circle to the given array

	{
	int i;

	if (iRadius <= 0)
		return;

	//	Make sure that we don't already have a circle like this

	for (i = 0; i < Array.GetCount(); i++)
		{
		SCircle &Test = Array[i];
		if (Test.x == x && Test.y == y)
			{
			if (Test.iRadius2 < (iRadius * iRadius))
				{
				Test.iRadius2 = (iRadius * iRadius);
				AddToBounds(x - iRadius, y + iRadius, x + iRadius, y - iRadius);
				}

			return;
			}
		}

	//	Add it

	SCircle *pCircle = Array.Insert();
	pCircle->x = x;
	pCircle->y = y;
	pCircle->iRadius2 = iRadius * iRadius;

	AddToBounds(x - iRadius, y + iRadius, x + iRadius, y - iRadius);
	}

void CComplexArea::AddRect (TArray<SRect> &Array, int x, int y, int cxWidth, int cyHeight, int iRotation)

//	AddRect
//
//	Adds the rect

	{
	int i;

	if (cxWidth <= 0 || cyHeight <= 0)
		return;

	//	See if we already have a rect like this

	for (i = 0; i < Array.GetCount(); i++)
		{
		SRect &Test = Array[i];
		if (Test.x == x 
				&& Test.y == y 
				&& Test.iRotation == iRotation 
				&& Test.cxWidth == cxWidth 
				&& Test.cyHeight == cyHeight)
			return;
		}

	//	Add it

	SRect *pRect = Array.Insert();
	pRect->x = x;
	pRect->y = y;
	pRect->cxWidth = cxWidth;
	pRect->cyHeight = cyHeight;
	pRect->iRotation = (iRotation > 0 ? (iRotation % 360) : 0);

	//	Add to bounds

	if (iRotation > 0)
		{
		int xLL = 0;
		int yLL = 0;

		int xLR, yLR;
		IntPolarToVector(iRotation, cxWidth, &xLR, &yLR);

		int xUL, yUL;
		IntPolarToVector(iRotation + 90, cyHeight, &xUL, &yUL);

		int xUR = xUL + xLR;
		int yUR = yUL + yLR;

		int xLeft = Min(Min(xLL, xLR), Min(xUL, xUR));
		int xRight = Max(Max(xLL, xLR), Max(xUL, xUR));
		int yTop = Max(Max(yLL, yLR), Max(yUL, yUR));
		int yBottom = Min(Min(yLL, yLR), Min(yUL, yUR));

		AddToBounds(x + xLeft, y + yTop, x + xRight, y + yBottom);
		}
	else
		AddToBounds(x, y + cyHeight, x + cxWidth, y);
	}

void CComplexArea::AddToBounds (int xLeft, int yTop, int xRight, int yBottom)

//	AddToBounds
//
//	Add to the bounds

	{
	if (xLeft < m_rcBounds.left)
		m_rcBounds.left = xLeft;

	if (xRight > m_rcBounds.right)
		m_rcBounds.right = xRight;

	if (yTop > m_rcBounds.top)
		m_rcBounds.top = yTop;

	if (yBottom < m_rcBounds.bottom)
		m_rcBounds.bottom = yBottom;
	}

bool CComplexArea::GeneratePointsInArea (int iCount, int iMinSeparation, CIntGraph *retGraph)

//	GeneratePointsInArea
//
//	Generates a set of points inside the area. Optionally ensures that each point
//	has a minimum separation from the others.
//
//	The array is always filled with the proper number of points, but if FALSE is returned,
//	then some points don't meet the required constraints

	{
	int i;

	//	Optimize 0

	retGraph->DeleteAll();
	if (iCount <= 0)
		return true;

	//	Generate points

	TArray<int> xs;
	TArray<int> ys;

	bool bConstraints = GeneratePointsInArea(iCount, iMinSeparation, &xs, &ys);
	ASSERT(xs.GetCount() == iCount && ys.GetCount() == iCount);

	//	Add to graph

	for (i = 0; i < iCount; i++)
		retGraph->AddNode(xs[i], ys[i]);

	//	Done

	return bConstraints;
	}

bool CComplexArea::GeneratePointsInArea (int iCount, int iMinSeparation, TArray<int> *retX, TArray<int> *retY)

//	GeneratePointsInArea
//
//	Generates a set of points inside the area. Optionally ensures that each point
//	has a minimum separation from the others.
//
//	The array is always filled with the proper number of points, but if FALSE is returned,
//	then some points don't meet the required constraints

	{
	int i, j;

	//	Optimize 0

	retX->DeleteAll();
	retY->DeleteAll();
	if (iCount <= 0)
		return true;

	//	Generate the requisite number of points

	bool bConstraints = true;
	retX->InsertEmpty(iCount);
	retY->InsertEmpty(iCount);
	for (i = 0; i < iCount; i++)
		if (!RandomPointInArea(&retX->GetAt(i), &retY->GetAt(i)))
			bConstraints = false;

	//	Now randomly adjust the positions until all have the minimum separation

	if (iMinSeparation > 0)
		{
		int iMinDist2 = iMinSeparation * iMinSeparation;
		int iTries = 100 * iCount;
		while (iTries > 0)
			{
			bool bNodeMoved = false;
			bool bTooClose = false;

			for (i = 0; i < iCount; i++)
				{
				//	Compute a vector pointing away from any close nodes

				int xRepel = 0;
				int yRepel = 0;
				for (j = 0; j < iCount; j++)
					if (j != i)
						{
						int xDist = (retX->GetAt(j) - retX->GetAt(i));
						int yDist = (retY->GetAt(j) - retY->GetAt(i));

						if (xDist == 0 && yDist == 0)
							{
							xRepel += (mathRandom(1, 2) == 1 ? 1 : -1);
							yRepel += (mathRandom(1, 2) == 1 ? 1 : -1);
							}
						else if ((xDist * xDist) + (yDist * yDist) < iMinDist2)
							{
							xRepel -= xDist;
							yRepel -= yDist;
							}
						}

				//	If we're too close to some nodes, move away

				if (xRepel != 0 || yRepel != 0)
					{
					int iRadius = Max(1, Max(Absolute(xRepel), Absolute(yRepel)) / 2);
					int xNew = retX->GetAt(i) + (xRepel / iRadius);
					int yNew = retY->GetAt(i) + (yRepel / iRadius);

					if (InArea(xNew, yNew))
						{
						retX->GetAt(i) = xNew;
						retY->GetAt(i) = yNew;

						bNodeMoved = true;
						}

					bTooClose = true;
					}
				}

			//	If none of the nodes moved, then it means either that all nodes
			//	are in place or that none were able to move. Either way, we're done

			if (!bNodeMoved)
				{
				if (bTooClose)
					bConstraints = false;
				return bConstraints;
				}

			//	Otherwise, we loop

			iTries--;
			}

		//	If we get this far, then we couldn't do it

		bConstraints = false;
		}

	//	Done

	return bConstraints;
	}

bool CComplexArea::InArea (int x, int y)

//	InArea
//
//	Returns TRUE if the given point is in the included areas and not in the
//	excluded ones

	{
	int i;
	bool bIncluded = false;

	for (i = 0; i < m_IncludedCircles.GetCount(); i++)
		if (InCircle(m_IncludedCircles[i], x, y))
			{
			bIncluded = true;
			break;
			}

	if (!bIncluded)
		{
		for (i = 0; i < m_IncludedRects.GetCount(); i++)
			if (InRect(m_IncludedRects[i], x, y))
				{
				bIncluded = true;
				break;
				}
		}

	if (!bIncluded)
		return false;

	//	See if we are excluded

	for (i = 0; i < m_ExcludedCircles.GetCount(); i++)
		if (InCircle(m_ExcludedCircles[i], x, y))
			return false;

	for (i = 0; i < m_ExcludedRects.GetCount(); i++)
		if (InRect(m_ExcludedRects[i], x, y))
			return false;

	//	Done

	return true;
	}

bool CComplexArea::InCircle (SCircle &Circle, int x, int y)

//	InCircle
//
//	Returns TRUE if the given point is in the circle

	{
	//	Convert to circle coordinates

	x = x - Circle.x;
	y = y - Circle.y;

	return ((x * x + y * y) <= Circle.iRadius2);
	}

bool CComplexArea::InRect (SRect &Rect, int x, int y)

//	InRect
//
//	Returns TRUE if the given point is in the rect

	{
	//	Convert x,y to rect coordinates

	x = x - Rect.x;
	y = y - Rect.y;

	//	Adjust for rotation

	if (Rect.iRotation > 0)
		{
		int iAngle = 360 - Rect.iRotation;
		int xRot = (int)((x * g_Cosine[iAngle] - y * g_Sine[iAngle]) + 0.5);
		int yRot = (int)((x * g_Sine[iAngle] + y * g_Cosine[iAngle]) + 0.5);
		return (xRot >= 0 && xRot < Rect.cxWidth && yRot >= 0 && yRot < Rect.cyHeight);
		}
	else
		return (x >= 0 && x < Rect.cxWidth && y >= 0 && y < Rect.cyHeight);
	}

void CComplexArea::Paint (CG32bitImage &Dest, int xCenter, int yCenter, Metric rScale) const

//	Paint
//
//	Paint the area

	{
	int i;

	for (i = 0; i < m_IncludedCircles.GetCount(); i++)
		PaintCircle(Dest, xCenter, yCenter, rScale, m_IncludedCircles[i], RGB_INCLUDED);

	for (i = 0; i < m_IncludedRects.GetCount(); i++)
		PaintRect(Dest, xCenter, yCenter, rScale, m_IncludedRects[i], RGB_INCLUDED);

	for (i = 0; i < m_ExcludedCircles.GetCount(); i++)
		PaintCircle(Dest, xCenter, yCenter, rScale, m_ExcludedCircles[i], RGB_EXCLUDED);

	for (i = 0; i < m_ExcludedRects.GetCount(); i++)
		PaintRect(Dest, xCenter, yCenter, rScale, m_ExcludedRects[i], RGB_EXCLUDED);
	}

void CComplexArea::PaintCircle (CG32bitImage &Dest, int xCenter, int yCenter, Metric rScale, const SCircle &Circle, CG32bitPixel rgbColor) const

//	PaintCircle
//
//	Paints a circle

	{
	int xPos = XToImageCoords(xCenter, Circle.x, rScale);
	int yPos = YToImageCoords(yCenter, Circle.y, rScale);
	int iRadius = mathRound(sqrt((Metric)Circle.iRadius2) * rScale);

	CGDraw::Arc(Dest, xPos, yPos, iRadius, 0, 0, 1, rgbColor);
	}

void CComplexArea::PaintRect (CG32bitImage &Dest, int xCenter, int yCenter, Metric rScale, const SRect &Rect, CG32bitPixel rgbColor) const

//	PaintRect
//
//	Paints a rect

	{
	CVector vWidth(rScale * Rect.cxWidth, 0.0);
	vWidth = vWidth.Rotate(Rect.iRotation);
	vWidth.SetY(-vWidth.GetY());

	CVector vHeight(0.0, rScale * Rect.cyHeight);
	vHeight = vHeight.Rotate(Rect.iRotation);
	vHeight.SetY(-vHeight.GetY());

	CVector vPos(XToImageCoords(xCenter, Rect.x, rScale), YToImageCoords(yCenter, Rect.y, rScale));
	CVector vUL = vPos;
	CVector vUR = vPos + vWidth;
	CVector vLL = vPos + vHeight;
	CVector vLR = vPos + vWidth + vHeight;

	int x = mathRound(vPos.GetX());
	int y = mathRound(vPos.GetY());

	CGDraw::Line(Dest, (int)vUL.GetX(), (int)vUL.GetY(), (int)vUR.GetX(), (int)vUR.GetY(), 1, rgbColor);
	CGDraw::Line(Dest, (int)vUR.GetX(), (int)vUR.GetY(), (int)vLR.GetX(), (int)vLR.GetY(), 1, rgbColor);
	CGDraw::Line(Dest, (int)vLR.GetX(), (int)vLR.GetY(), (int)vLL.GetX(), (int)vLL.GetY(), 1, rgbColor);
	CGDraw::Line(Dest, (int)vLL.GetX(), (int)vLL.GetY(), (int)vUL.GetX(), (int)vUL.GetY(), 1, rgbColor);
	}

bool CComplexArea::RandomPointInArea (int *retx, int *rety)

//	RandomPointInArea
//
//	Attempts to return a random point inside the area. If successful, returns TRUE

	{
	int iTries = MAX_TRIES;
	while (iTries > 0)
		{
		*retx = mathRandom(m_rcBounds.left, m_rcBounds.right);
		*rety = mathRandom(m_rcBounds.bottom, m_rcBounds.top);

		if (InArea(*retx, *rety))
			return true;

		iTries--;
		}

	return false;
	}

void CComplexArea::ReadCircleArray (IReadStream &Stream, TArray<SCircle> &Result)

//	ReadCircleArray
//
//	DWORD			No. of included circles
//	SCircle

	{
	int i;

	DWORD dwCount;
	Stream.Read(dwCount);

	Result.InsertEmpty(dwCount);
	for (i = 0; i < Result.GetCount(); i++)
		Stream.Read((char *)&Result[i], sizeof(SCircle));
	}

void CComplexArea::ReadRectArray (IReadStream &Stream, TArray<SRect> &Result)

//	ReadRectArray
//
//	DWORD			No. of included rects
//	SRect

	{
	int i;

	DWORD dwCount;
	Stream.Read(dwCount);

	Result.InsertEmpty(dwCount);
	for (i = 0; i < Result.GetCount(); i++)
		Stream.Read((char *)&Result[i], sizeof(SRect));
	}

void CComplexArea::ReadFromStream (IReadStream &Stream)

//	ReadFromStream
//
//	Reads the complex area
//
//	DWORD			Flags
//	RECT			m_rcBounds
//
//	DWORD			No. of included rects
//	SRect
//
//	DWORD			No. of included circles
//	SCircle
//
//	DWORD			No. of excluded rects
//	SRect
//
//	DWORD			No. of excluded circles
//	SCircle

	{
	DWORD dwFlags;
	Stream.Read(dwFlags);

	Stream.Read((char *)&m_rcBounds, sizeof(RECT));

	ReadRectArray(Stream, m_IncludedRects);
	ReadCircleArray(Stream, m_IncludedCircles);
	ReadRectArray(Stream, m_ExcludedRects);
	ReadCircleArray(Stream, m_ExcludedCircles);
	}

void CComplexArea::WriteCircleArray (IWriteStream &Stream, const TArray<SCircle> &Array) const

//	WriteCircleArray
//
//	DWORD			No. of included circles
//	SCircle

	{
	int i;

	DWORD dwCount = Array.GetCount();;
	Stream.Write(dwCount);

	for (i = 0; i < Array.GetCount(); i++)
		Stream.Write((char *)&Array[i], sizeof(SCircle));
	}

void CComplexArea::WriteRectArray (IWriteStream &Stream, const TArray<SRect> &Array) const

//	WriteRectArray
//
//	DWORD			No. of included rects
//	SRect

	{
	int i;

	DWORD dwCount = Array.GetCount();;
	Stream.Write(dwCount);

	for (i = 0; i < Array.GetCount(); i++)
		Stream.Write((char *)&Array[i], sizeof(SRect));
	}

void CComplexArea::WriteToStream (IWriteStream &Stream) const

//	ReadFromStream
//
//	Write the complex area

	{
	DWORD dwFlags = 1;
	Stream.Write(dwFlags);

	Stream.Write((char *)&m_rcBounds, sizeof(RECT));

	WriteRectArray(Stream, m_IncludedRects);
	WriteCircleArray(Stream, m_IncludedCircles);
	WriteRectArray(Stream, m_ExcludedRects);
	WriteCircleArray(Stream, m_ExcludedCircles);
	}

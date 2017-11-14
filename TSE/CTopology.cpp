//	CTopology.cpp
//
//	Star system topology

#include "PreComp.h"

#define AREA_TAG								CONSTLIT("Area")
#define ENTRANCE_NODE_TAG						CONSTLIT("FragmentEntrance")
#define FRAGMENT_TAG							CONSTLIT("Fragment")
#define LINE_TAG								CONSTLIT("Line")
#define NETWORK_TAG								CONSTLIT("Network")
#define NODE_TAG								CONSTLIT("Node")
#define NODES_TAG								CONSTLIT("Nodes")
#define NODE_TABLE_TAG							CONSTLIT("NodeTable")
#define NODE_TEMPLATE_TAG						CONSTLIT("NodeTemplate")
#define SYSTEM_TAG								CONSTLIT("System")
#define STARGATE_TAG							CONSTLIT("Stargate")
#define STARGATES_TAG							CONSTLIT("Stargates")
#define STARGATE_TABLE_TAG						CONSTLIT("StargateTable")

#define ATTRIBUTES_ATTRIB						CONSTLIT("attributes")
#define CHANCE_ATTRIB							CONSTLIT("chance")
#define COUNT_ATTRIB							CONSTLIT("count")
#define DEBUG_ATTRIB							CONSTLIT("debug")
#define DEBUG_ONLY_ATTRIB						CONSTLIT("debugOnly")
#define DEST_FRAGMENT_ATTRIBUTES_ATTRIB			CONSTLIT("destFragmentAttributes")
#define DEST_FRAGMENT_EXIT_ATTRIB				CONSTLIT("destFragmentExit")
#define DEST_FRAGMENT_ROTATION_ATTRIB			CONSTLIT("destFragmentRotation")
#define DESTGATE_ATTRIB							CONSTLIT("destGate")
#define DESTID_ATTRIB							CONSTLIT("destID")
#define FROM_ATTRIB								CONSTLIT("from")
#define ID_ATTRIB								CONSTLIT("ID")
#define LEVEL_ATTRIB							CONSTLIT("level")
#define MIN_SEPARATION_ATTRIB					CONSTLIT("minSeparation")
#define NAME_ATTRIB								CONSTLIT("name")
#define NODE_ID_ATTRIB							CONSTLIT("nodeID")
#define ONE_WAY_ATTRIB							CONSTLIT("oneWay")
#define PATH_ATTRIB								CONSTLIT("path")
#define RADIUS_ATTRIB							CONSTLIT("radius")
#define ROOT_NODE_ATTRIB						CONSTLIT("rootNode")
#define TO_ATTRIB								CONSTLIT("to")
#define UNID_ATTRIB								CONSTLIT("UNID")
#define VARIANT_ATTRIB							CONSTLIT("variant")

#define FRAGMENT_ENTRANCE_DEST					CONSTLIT("[FragmentEntrance]")
#define FRAGMENT_EXIT_DEST						CONSTLIT("[FragmentExit]")
#define PREV_DEST								CONSTLIT("[Prev]")

const int DEFAULT_MIN_SEPARATION =				40;
const int INFINITE_DISTANCE =					2000000000;
const int UNKNOWN_DISTANCE =					-1;

CTopology::CTopology (void)

//	CTopology constructor

	{
	}

CTopology::~CTopology (void)

//	CTopology destructor

	{
	DeleteAll();
	}

ALERROR CTopology::AddFragment (STopologyCreateCtx &Ctx, CTopologyDesc *pFragment, CTopologyNode **retpNewNode)

//	AddFragment
//
//	Adds the given fragment. The appropriate fields in Ctx must be set up.

	{
	ALERROR error;

	CXMLElement *pFragmentXML = pFragment->GetDesc();
	CTopologyDescTable *pFragTable = pFragment->GetTopologyDescTable();

	//	Must have a root node

	if (pFragTable->GetRootNodeCount() == 0)
		{
		Ctx.sError = strPatternSubst(CONSTLIT("Fragment %s: No root node found."), pFragment->GetID());
		return ERR_FAIL;
		}

	CTopologyDesc *pRootNode = pFragTable->GetRootNodeDesc(0);
	ASSERT(pRootNode);

	//	Generate a unique prefix

	CString sPartialID = pRootNode->GetID();
	CString sPrefix = pFragment->GetID();

	CString sOriginalPrefix = sPrefix;
	int iNumber = 2;
	while (FindTopologyNode(strPatternSubst(CONSTLIT("%s%s"), sPrefix, sPartialID)))
		{
		sPrefix = strPatternSubst(CONSTLIT("%s%d"), sOriginalPrefix, iNumber++);
		}

	//	Prepare context

	STopologyCreateCtx NewCtx;
	NewCtx = Ctx;
	NewCtx.bInFragment = true;
	NewCtx.pFragmentTable = pFragTable;
	NewCtx.sFragmentPrefix = sPrefix;

	//	Add the root node

	if (error = AddTopologyDesc(NewCtx, pRootNode, retpNewNode))
		{
		Ctx.sError = NewCtx.sError;
		return error;
		}

	return NOERROR;
	}

ALERROR CTopology::AddNetwork (STopologyCreateCtx &Ctx, CTopologyDesc *pNetwork, CTopologyNode **retpNewNode)

//	AddNetwork
//
//	Adds a node network

	{
	ALERROR error;
	int i;

	CXMLElement *pNetworkXML = pNetwork->GetDesc();

	//	Initialize to NULL (in case we exit early with no nodes).

	if (retpNewNode)
		*retpNewNode = NULL;

	//	Generate a unique prefix for the nodes in this network

	CString sPrefix = pNetworkXML->GetAttribute(ID_ATTRIB);
	if (!sPrefix.IsBlank() && pNetwork->GetTopologyDescCount() > 0)
		{
		CString sPartialID = pNetwork->GetTopologyDesc(0)->GetID();
		CString sOriginalPrefix = sPrefix;
		int iNumber = 2;
		while (FindTopologyNode(strPatternSubst(CONSTLIT("%s%s"), sPrefix, sPartialID)))
			{
			sPrefix = strPatternSubst(CONSTLIT("%s%d"), sOriginalPrefix, iNumber++);
			}
		}

	//	Prepare context

	STopologyCreateCtx NewCtx;
	NewCtx = Ctx;
	//	Ideally, the fragment property would be set by the caller, but until
	//	then we assume that all <Network> elements are fragments
	NewCtx.bInFragment = true;
	NewCtx.pFragmentTable = NULL;
	NewCtx.sFragmentPrefix = sPrefix;

	//	Add all the nodes in the network

	for (i = 0; i < pNetwork->GetTopologyDescCount(); i++)
		{
		CTopologyDesc *pNodeDesc = pNetwork->GetTopologyDesc(i);

		//	If this is a relative node, of if the node doesn't exist,
		//	then we need to create it.

		CTopologyNode *pNewNode;
		if (!pNodeDesc->IsAbsoluteNode()
				|| (pNewNode = FindTopologyNode(pNodeDesc->GetID())) == NULL)
			{
			if (error = AddTopologyDesc(NewCtx, pNodeDesc, &pNewNode))
				{
				Ctx.sError = NewCtx.sError;
				return error;
				}
			}

		//	We return the first node

		if (i == 0 && retpNewNode)
			*retpNewNode = pNewNode;
		}

	//	Now create the stargate connections

	CXMLElement *pStargateList = pNetworkXML->GetContentElementByTag(STARGATES_TAG);
	if (pStargateList == 0 || pStargateList->GetContentElementCount() == 0)
		return NOERROR;

	//	Always treat it as a group

	IElementGenerator::SCtx GenCtx;
	GenCtx.pTopology = this;

	TArray<CXMLElement *> Stargates;
	CString sError;
	if (!IElementGenerator::GenerateAsGroup(GenCtx, pStargateList, Stargates, &sError))
		{
		Ctx.sError = strPatternSubst(CONSTLIT("Topology %s: Unable to generate random stargate table: %s"), pNetworkXML->GetAttribute(ID_ATTRIB), sError);
		return ERR_FAIL;
		}

	for (i = 0; i < Stargates.GetCount(); i++)
		{
		CXMLElement *pGate = Stargates[i];

		//	If this is a directive to set the return node, then process it

		if (strEquals(pGate->GetTag(), ENTRANCE_NODE_TAG))
			{
			CString sNodeID = pGate->GetAttribute(NODE_ID_ATTRIB);
			CTopologyNode *pEntranceNode = FindTopologyNode(ExpandNodeID(NewCtx, sNodeID));
			if (pEntranceNode == NULL)
				{
				Ctx.sError = strPatternSubst(CONSTLIT("Topology %s: Unable to find entrance node: %s"), pNetworkXML->GetAttribute(ID_ATTRIB), sNodeID);
				return ERR_FAIL;
				}

			if (retpNewNode)
				*retpNewNode = pEntranceNode;
			}

		//	This will add the stargate and recurse into AddTopologyDesc
		//	(if necessary).

		else
			{
			if (error = AddStargate(NewCtx, NULL, false, pGate))
				{
				Ctx.sError = NewCtx.sError;
				return error;
				}
			}
		}

	return NOERROR;
	}

ALERROR CTopology::AddNode (STopologyCreateCtx &Ctx, CTopologyDesc *pNode, CTopologyNode **retpNewNode)

//	AddNode
//
//	Adds a node and recurses to add any nodes that it points to

	{
	ALERROR error;
	int i;

	CXMLElement *pNodeXML = pNode->GetDesc();
	CString sID = pNode->GetID();
	bool bIsRootNode = pNode->IsRootNode();

	int xPos, yPos;
	bool bHasPos = pNode->GetPos(&xPos, &yPos);

	//	Create a topology node and add it to the universe list

	SNodeCreateCtx CreateCtx;
	CreateCtx.xPos = xPos;
	CreateCtx.yPos = yPos;
	CreateCtx.sAttribs = pNode->GetAttributes();
	CreateCtx.pSystemDesc = pNode->GetSystem();
	CreateCtx.pEffect = pNode->GetMapEffect();
	CreateCtx.iInitialState = pNode->GetInitialState();
	CreateCtx.bNoMap = !bHasPos;

	CTopologyNode *pNewNode;
	if (error = CreateTopologyNode(Ctx, ExpandNodeID(Ctx, sID), CreateCtx, &pNewNode))
		return error;

	//	Loop over remaining elements to see if we have stargate elements

	for (i = 0; i < pNodeXML->GetContentElementCount(); i++)
		{
		CXMLElement *pItem = pNodeXML->GetContentElement(i);

		//	If <Stargates> or <Stargate> then add

		if (strEquals(pItem->GetTag(), STARGATES_TAG) || strEquals(pItem->GetTag(), STARGATE_TAG))
			{
			if (error = AddStargateFromXML(Ctx, pItem, pNewNode, bIsRootNode))
				{
				Ctx.sError = strPatternSubst(CONSTLIT("Node %s: %s"), pNode->GetID(), Ctx.sError);
				return error;
				}
			}

		//	OK if we have other nodes that we don't handle here (e.g., <Effect>)
		}

	//	Done

	if (retpNewNode)
		*retpNewNode = pNewNode;

	return NOERROR;
	}

ALERROR CTopology::AddNodeGroup (STopologyCreateCtx &Ctx, CTopologyDesc *pTable, CTopologyNode **retpNewNode)

//	AddNodeGroup
//
//	Adds a node listed here

	{
	ALERROR error;
	int i;

	CXMLElement *pTableXML = pTable->GetDesc();

	//	Loop over all items

	for (i = 0; i < pTableXML->GetContentElementCount(); i++)
		{
		CXMLElement *pEntry = pTableXML->GetContentElement(i);

		//	Is this a stargate reference?

		if (strEquals(pEntry->GetTag(), STARGATE_TAG) 
				|| strEquals(pEntry->GetTag(), STARGATES_TAG)
				|| strEquals(pEntry->GetTag(), STARGATE_TABLE_TAG))
			{
			if (error = AddStargateFromXML(Ctx, pEntry))
				{
				Ctx.sError = strPatternSubst(CONSTLIT("%s: %s"), pTable->GetID(), Ctx.sError);
				return error;
				}
			}

		//	Is this a node reference?

		else if (strEquals(pEntry->GetTag(), NODE_TAG))
			{
			if (error = AddTopologyNode(Ctx, pEntry->GetAttribute(ID_ATTRIB)))
				{
				Ctx.sError = strPatternSubst(CONSTLIT("%s: %s"), pTable->GetID(), Ctx.sError);
				return error;
				}
			}

		//	Otherwise, we don't know what this is

		else
			{
			Ctx.sError = strPatternSubst(CONSTLIT("%s: Unknown NodeGroup directive: %s."), pTable->GetID(), pEntry->GetTag());
			return ERR_FAIL;
			}
		}

	return NOERROR;
	}

ALERROR CTopology::AddNodeTable (STopologyCreateCtx &Ctx, CTopologyDesc *pTable, CTopologyNode **retpNewNode)

//	AddNodeTable
//
//	Adds a node by looking up in a table

	{
	ALERROR error;
	int i;

	CXMLElement *pTableXML = pTable->GetDesc();

	bool bUnique = true;

	//	Create a table of probabilities

	struct SEntry
		{
		int iChance;
		CString sNodeID;
		};

	int iTotal = 0;
	TArray<SEntry> Table;
	for (i = 0; i < pTableXML->GetContentElementCount(); i++)
		{
		CXMLElement *pEntry = pTableXML->GetContentElement(i);
		int iChance = pEntry->GetAttributeIntegerBounded(CHANCE_ATTRIB, 1);
		CString sNodeID = pEntry->GetAttribute(ID_ATTRIB);
		if (sNodeID.IsBlank())
			{
			Ctx.sError = strPatternSubst(CONSTLIT("NodeTable %s: ID expected for node."), pTable->GetID());
			return ERR_FAIL;
			}

		//	If this node has already been created and we only want
		//	new nodes, then continue

		if (bUnique && FindTopologyNode(sNodeID))
			continue;

		//	Insert in the table

		SEntry *pNewEntry = Table.Insert();
		pNewEntry->iChance = iChance;
		pNewEntry->sNodeID = sNodeID;

		//	Add up totals

		iTotal += iChance;
		}

	//	If we have nothing, then we have an error

	if (iTotal == 0 || Table.GetCount() == 0)
		{
		Ctx.sError = strPatternSubst(CONSTLIT("NodeTable %s: No more nodes available."), pTable->GetID());
		return ERR_FAIL;
		}

	//	Roll a random number

	int iRoll = mathRandom(1, iTotal);
	CString sNodeID;
	for (i = 0; i < Table.GetCount(); i++)
		if (iRoll <= Table[i].iChance)
			{
			sNodeID = Table[i].sNodeID;
			break;
			}
		else
			iRoll -= Table[i].iChance;

	ASSERT(!sNodeID.IsBlank());

	//	Look for the node

	CTopologyDesc *pDesc;
	if (error = FindTopologyDesc(Ctx, sNodeID, &pDesc))
		return error;

	//	Add it

	if (error = AddTopologyDesc(Ctx, pDesc, retpNewNode))
		return error;

	return NOERROR;
	}

ALERROR CTopology::AddRandom (STopologyCreateCtx &Ctx, CTopologyDesc *pDesc, CTopologyNode **retpNewNode)

//	AddRandom
//
//	Adds a random node network

	{
	ALERROR error;
	int i, j;

	//	If we're not in a fragment, see if we've already added some nodes for 
	//	this descriptor. If so, then we return the nearest existing node to the
	//	previous node.

	if (!Ctx.bInFragment 
			&& Ctx.pPrevNode
			&& FindNearestNodeCreatedBy(pDesc->GetID(), Ctx.pPrevNode, retpNewNode))
		return NOERROR;

	//	Initialize

	CXMLElement *pXML = pDesc->GetDesc();
	if (retpNewNode)
		*retpNewNode = NULL;

	//	Create new nodes by region. We end up with the following:
	//
	//	Nodes: A single array of created topology nodes across all regions.
	//	Graph: A single graph containing all nodes across all regions.
	//	pExitNode: The topology node that we exit to (if we're a fragment and there is an exit)

	TArray<CTopologyNode *> Nodes;
	CIntGraph Graph;
	CTopologyNode *pExitNode = NULL;

	if (error = AddRandomRegion(Ctx, pDesc, pXML, pExitNode, Graph, Nodes))
		return error;

	//	Remember the list of nodes

	int iNodeCount = Nodes.GetCount();
	if (iNodeCount == 0)
		{
		if (retpNewNode)
			*retpNewNode = NULL;
		return NOERROR;
		}

	//	Get the node that is closest to the entrance node

	int xEntrance, yEntrance;
	GetFragmentEntranceDisplayPos(Ctx, &xEntrance, &yEntrance);

	DWORD dwFirstNode;
	Graph.FindNearestNode(xEntrance, yEntrance, &dwFirstNode);

	//	Generate random connections between the nodes

	Graph.GenerateRandomConnections(dwFirstNode, 1, 10);

	//	Now connect the nodes based on the connections in the randomly
	//	generated graph.

	for (i = 0; i < iNodeCount; i++)
		{
		CTopologyNode *pFrom = Nodes[i];

		TArray<int> To;
		Graph.GetNodeForwardConnections(Graph.GetNodeID(i), &To);

		for (j = 0; j < To.GetCount(); j++)
			{
			CTopologyNode *pTo = Nodes[Graph.GetNodeIndex(To[j])];
#if 0
			int iFromCount = pFrom->GetStargateCount();
			int iToCount = pTo->GetStargateCount();
#endif

			//	Create both stargates with autogenerated names

			CTopologyNode::SStargateRouteDesc RouteDesc;
			RouteDesc.pFromNode = pFrom;
			RouteDesc.pToNode = pTo;

			if (error = CTopologyNode::CreateStargateRoute(RouteDesc))
				::kernelDebugLogPattern("Error creating a stargate in <Random> generation.");
			}
		}

	//	Connect to the exit node

	if (pExitNode)
		{
		//	Get the position of the exit node

		int xExit, yExit;
		GetFragmentDisplayPos(Ctx, pExitNode, &xExit, &yExit);

		//	Find the nearest node to the exit

		DWORD dwLastNode;
		Graph.FindNearestNode(xExit, yExit, &dwLastNode);

		CTopologyNode *pLastNode = Nodes[Graph.GetNodeIndex(dwLastNode)];

		//	Connect to the exit node (both directions, with autogenerated names)

		CTopologyNode::SStargateRouteDesc RouteDesc;
		RouteDesc.pFromNode = pLastNode;
		RouteDesc.pToNode = pExitNode;

		if (error = CTopologyNode::CreateStargateRoute(RouteDesc))
			::kernelDebugLogPattern("Error creating a stargate in <Random> generation.");
		}

	//	See if we have an effect (and if so, add it)

	CEffectCreator *pEffect = pDesc->GetMapEffect();
	if (pEffect)
		{
		int xPos;
		int yPos;
		int iRotation;
		GetAbsoluteDisplayPos(Ctx, 0, 0, &xPos, &yPos, &iRotation);

		Ctx.pMap->AddAnnotation(pEffect, xPos, yPos, iRotation);
		}

	//	Return the first node

	if (retpNewNode)
		*retpNewNode = Nodes[Graph.GetNodeIndex(dwFirstNode)];

	//	Remember the nodes created in the descriptor (we need this in case we
	//	refer to this descriptor again).

	return NOERROR;
	}

ALERROR CTopology::AddRandomParsePosition (STopologyCreateCtx *pCtx, const CString &sValue, CTopologyNode **iopExit, int *retx, int *rety)

//	AddRandomParsePosition
//
//	Helper method for AddRandom

	{
	ALERROR error;

	if (pCtx && strEquals(sValue, FRAGMENT_ENTRANCE_DEST))
		{
		GetFragmentEntranceDisplayPos(*pCtx, retx, rety);
		}
	else if (pCtx && strEquals(sValue, FRAGMENT_EXIT_DEST))
		{
		if (*iopExit == NULL)
			{
			//	If we don't know the exit node, get it now

			if (pCtx->sFragmentExitID.IsBlank() || *pCtx->sFragmentExitID.GetASCIIZPointer() == '+')
				{
				pCtx->sError = strPatternSubst(CONSTLIT("Invalid fragment exit node for <Random>: %s"), pCtx->sFragmentExitID);
				return ERR_FAIL;
				}

			//	Get the node

			if (error = GetOrAddTopologyNode(*pCtx, pCtx->sFragmentExitID, NULL, NULL, iopExit))
				return error;

			//	Get the position

			if (*iopExit)
				GetFragmentDisplayPos(*pCtx, (*iopExit), retx, rety);
			else
				{
				*retx = 0;
				*rety = 0;
				}
			}
		}
	else
		{
		if (error = CTopologyNode::ParsePosition(sValue, retx, rety))
			return error;
		}

	return NOERROR;
	}

ALERROR CTopology::AddRandomRegion (STopologyCreateCtx &Ctx, 
									CTopologyDesc *pDesc, 
									CXMLElement *pRegionDef, 
									CTopologyNode *&pExitNode, 
									CIntGraph &Graph, 
									TArray<CTopologyNode *> &Nodes)

//	AddRandomRegion
//
//	Adds random nodes to a region in a <Random> definition

	{
	ALERROR error;
	int i, j;

	//	Figure out how many nodes to create

	DiceRange Count;
	if (error = Count.LoadFromXML(pRegionDef->GetAttribute(COUNT_ATTRIB)))
		{
		Ctx.sError = strPatternSubst(CONSTLIT("%s: Invalid count attribute: %s"), pDesc->GetID(), pRegionDef->GetAttribute(COUNT_ATTRIB));
		return error;
		}

	int iNodeCount = Count.Roll();
	if (iNodeCount <= 0)
		return NOERROR;

	//	Get the minimum separation between nodes

	int iMinNodeDist = pRegionDef->GetAttributeIntegerBounded(MIN_SEPARATION_ATTRIB, 1, -1, DEFAULT_MIN_SEPARATION);

	//	Get the area definition

	CXMLElement *pAreaDef = pRegionDef->GetContentElementByTag(AREA_TAG);
	if (pAreaDef == NULL)
		{
		Ctx.sError = strPatternSubst(CONSTLIT("%s: Unable to find <Area> in <Random> element."), pDesc->GetID());
		return ERR_FAIL;
		}

	//	Initialize the area

	CComplexArea ValidArea;
	if (error = InitComplexArea(pAreaDef, iMinNodeDist, &ValidArea, &Ctx, &pExitNode))
		{
		Ctx.sError = strPatternSubst(CONSTLIT("%s: %s"), pDesc->GetID(), Ctx.sError);
		return error;
		}

	//	If we're debugging, then add the valid area as a highlight (so we can see it
	//	on the map)

	if (g_pUniverse->InDebugMode()
			&& pAreaDef->GetAttributeBool(DEBUG_ATTRIB))
		{
		Ctx.pMap->AddAreaHighlight(ValidArea);
		}

	//	Initialize an array of random positions within the valid area

	CIntGraph RegionGraph;
	ValidArea.GeneratePointsInArea(iNodeCount, iMinNodeDist, &RegionGraph);
	ASSERT(RegionGraph.GetNodeCount() == iNodeCount);

	//	Generate a unique prefix for this fragment

	CString sPrefix = GenerateUniquePrefix(pDesc->GetID(), CONSTLIT("+N0"));
	int iStart = Nodes.GetCount();

	//	Create the required number of nodes in the valid area

	for (i = 0; i < iNodeCount; i++)
		{
		//	Generate a unique node ID

		CString sNodeID = strPatternSubst(CONSTLIT("%s+N%d"), sPrefix, iStart + i);

		//	Get the node position

		int x, y;
		RegionGraph.GetNodePos(RegionGraph.GetNodeID(i), &x, &y);

		//	Add the node.
		//
		//	NOTE: We can use the same context because the caller sets whether this is 
		//	fragment or not. Also, we don't need to set sFragmentPrefix because this
		//	call does not need it (since we're creating everything manually).

		SNodeCreateCtx CreateCtx;
		CreateCtx.xPos = x;
		CreateCtx.yPos = y;
		CreateCtx.iInitialState = pDesc->GetInitialState();

		CTopologyNode *pNode;
		if (error = CreateTopologyNode(Ctx, sNodeID, CreateCtx, &pNode))
			return error;

		//	Mark this node as created by the given descriptor

		pNode->SetCreatorID(pDesc->GetID());

		//	Keep track of the node globally

		Nodes.Insert(pNode);

		//	Unmark the node to indicate that we need to set its system and attributes

		pNode->SetMarked(false);
		}

	//	Loop over all set nodes and initialize them appropriately

	for (i = 0; i < pDesc->GetTopologyDescCount(); i++)
		{
		CTopologyDesc *pSetNode = pDesc->GetTopologyDesc(i);

		if (pSetNode->GetType() != ndNode)
			{
			Ctx.sError = strPatternSubst(CONSTLIT("%s: Set nodes must be <Node> type."), pDesc->GetID());
			return ERR_FAIL;
			}

		//	Figure out the position of the set node (which are in fragment-relative coordinates)

		int x, y;
		pSetNode->GetPos(&x, &y);

		//	Now look for the nearest unmarked node to those coordinates

		int iBestDist2;
		CTopologyNode *pBestNode = NULL;
		for (j = 0; j < Nodes.GetCount(); j++)
			{
			CTopologyNode *pNode = Nodes[i];

			if (!pNode->IsMarked())
				{
				int xNode, yNode;
				GetFragmentDisplayPos(Ctx, pNode, &xNode, &yNode);

				int xDiff = (xNode - x);
				int yDiff = (yNode - y);
				int iDist2 = (xDiff * xDiff) + (yDiff * yDiff);

				if (pBestNode == NULL || iDist2 < iBestDist2)
					{
					pBestNode = pNode;
					iBestDist2 = iDist2;
					}
				}
			}

		//	If we found it, apply attributes, system, and effect

		if (pBestNode)
			{
			//	Attributes

			pBestNode->AddAttributes(pSetNode->GetAttributes());
			
			//	System

			CXMLElement *pSystemXML = pSetNode->GetSystem();
			if (pSystemXML)
				{
				if (error = pBestNode->InitFromSystemXML(*this, pSystemXML, &Ctx.sError))
					return error;
				}

			//	Effect

			CEffectCreator *pEffect = pSetNode->GetMapEffect();
			if (pEffect)
				{
				int xMap, yMap;

				pBestNode->GetDisplayPos(&xMap, &yMap);
				int iRotation = (Ctx.bInFragment ? Ctx.iRotation : 0);

				Ctx.pMap->AddAnnotation(pBestNode->GetID(), pEffect, xMap, yMap, iRotation);
				}

			//	Mark it, so we don't process it later

			pBestNode->SetMarked();
			}

		//	If we found no best node, then we exit, since we have run out of nodes

		else
			break;
		}

	//	Now loop over all unmarked nodes and take attributes from the node template

	CXMLElement *pNodeTemplate = pRegionDef->GetContentElementByTag(NODE_TEMPLATE_TAG);
	if (pNodeTemplate)
		{
		if (error = ApplyNodeTemplate(Ctx, Nodes, pNodeTemplate, true))
			return error;
		}

	//	Add the region graph to the overall graph

	Graph.AddGraph(RegionGraph);

	return NOERROR;
	}

ALERROR CTopology::AddStargateFromXML (STopologyCreateCtx &Ctx, CXMLElement *pDesc, CTopologyNode *pNode, bool bRootNode)

//	AddStargateFromXML
//
//	Adds a stargate a defined by a XML descriptor (which may also be a group of stargates)

	{
	ALERROR error;
	int i;

	//	If this is a <Stargates> tag or a <Stargate> tag with content
	//	elements, then treat it as a group.

	if (pDesc->GetContentElementCount() > 0)
		{
		IElementGenerator::SCtx GenCtx;
		GenCtx.pTopology = this;

		TArray<CXMLElement *> Stargates;
		CString sError;

		if (strEquals(pDesc->GetTag(), STARGATE_TABLE_TAG))
			{
			if (!IElementGenerator::GenerateAsTable(GenCtx, pDesc, Stargates, &sError))
				{
				Ctx.sError = strPatternSubst(CONSTLIT("Unable to generate random stargate table: %s"), sError);
				return ERR_FAIL;
				}
			}
		else
			{
			if (!IElementGenerator::GenerateAsGroup(GenCtx, pDesc, Stargates, &sError))
				{
				Ctx.sError = strPatternSubst(CONSTLIT("Unable to generate random stargate table: %s"), sError);
				return ERR_FAIL;
				}
			}

		for (i = 0; i < Stargates.GetCount(); i++)
			{
			CXMLElement *pGate = Stargates[i];

			//	This will add the stargate and recurse into AddTopologyDesc
			//	(if necessary).

			if (error = AddStargate(Ctx, pNode, bRootNode, pGate))
				return error;
			}
		}

	//	If we have a single <Stargate> tag, then just add a single stargate

	else if (strEquals(pDesc->GetTag(), STARGATE_TAG))
		{
		int iChance = pDesc->GetAttributeIntegerBounded(CHANCE_ATTRIB, 0, 100, 100);

		if (iChance == 100 || mathRandom(1, 100) <= iChance)
			{
			if (error = AddStargate(Ctx, pNode, bRootNode, pDesc))
				return error;
			}
		}
	else
		{
		Ctx.sError = strPatternSubst(CONSTLIT("Unknown stargate directive: %s"), pDesc->GetTag());
		return ERR_FAIL;
		}

	//	Done

	return NOERROR;
	}

ALERROR CTopology::AddStargate (STopologyCreateCtx &Ctx, CTopologyNode *pNode, bool bRootNode, CXMLElement *pGateDesc)

//	AddStargate
//
//	Adds a stargate as described inside a <Node> element

	{
	ALERROR error;

	//	If this is a debugOnly gate then only add it in debug mode

	if (pGateDesc->GetAttributeBool(DEBUG_ONLY_ATTRIB) && !g_pUniverse->InDebugMode())
		return NOERROR;

	//	There are two ways in which we get gate data:
	//
	//	1.	We are in a <Network> (pNode is NULL) and we have a <Stargate> element that
	//		connects two nodes.
	//
	//	2.	We are in a <Node> element (pNode is not NULL) and we have a <Stargate> element
	//		leading to another node

	CString sGateName;
	CString sDest;
	CString sDestEntryPoint;
	bool bOneWay;
	if (pNode == NULL)
		{
		CString sSource;
		CTopologyNode::ParseStargateString(pGateDesc->GetAttribute(FROM_ATTRIB), &sSource, &sGateName);
		CTopologyNode::ParseStargateString(pGateDesc->GetAttribute(TO_ATTRIB), &sDest, &sDestEntryPoint);
		bOneWay = pGateDesc->GetAttributeBool(ONE_WAY_ATTRIB);

		if (sSource.IsBlank())
			{
			Ctx.sError = CONSTLIT("Missing from= attribute in <Stargate> directive.");
			return ERR_FAIL;
			}

		if (sDest.IsBlank())
			{
			Ctx.sError = CONSTLIT("Missing to= attribute in <Stargate> directive.");
			return ERR_FAIL;
			}

		//	Find the source node

		if (error = GetOrAddTopologyNode(Ctx, sSource, NULL, pGateDesc, &pNode))
			return error;
		}
	else
		{
		//	Get basic data from the element

		sGateName = pGateDesc->GetAttribute(NAME_ATTRIB);

		CString sTo;
		if (pGateDesc->FindAttribute(TO_ATTRIB, &sTo))
			CTopologyNode::ParseStargateString(sTo, &sDest, &sDestEntryPoint);
		else
			{
			sDest = pGateDesc->GetAttribute(DESTID_ATTRIB);
			sDestEntryPoint = pGateDesc->GetAttribute(DESTGATE_ATTRIB);
			}

		bOneWay = pGateDesc->GetAttributeBool(ONE_WAY_ATTRIB);
		}

	//	Make sure we have a destination

	if (sDest.IsBlank())
		{
		Ctx.sError = strPatternSubst(CONSTLIT("Topology %s: Stargate destID required."), pNode->GetID());
		return ERR_FAIL;
		}

	//	At this point we have:
	//
	//	pNode is a valid source node
	//	sDest is a non-empty destination node
	//	sGateName is the name of the source gate (may be NULL)
	//	sDestEntryPoint is the name of the dest gate (may be NULL)
	//	bOneWay is TRUE if we only want a one-way gate

	//	If the destination is PREV_DEST, then we handle it

	if (strEquals(sDest, PREV_DEST))
		{
		//	If this is a root node, then we keep the "Prev" keyword
		//	(we will fix it up later, when the system actually gets created.)

		if (bRootNode)
			{
			//	In this case, we need a gate name

			if (sGateName.IsBlank())
				{
				Ctx.sError = strPatternSubst(CONSTLIT("Topology %s: Stargate name required."), pNode->GetID());
				return ERR_FAIL;
				}

			//	Add the stargate

			if (error = pNode->AddStargate(sGateName, sDest, NULL_STR, CTopologyNode::SStargateRouteDesc()))
				{
				//	A duplicate name is not an error--just a warning
				CString sWarning = strPatternSubst(CONSTLIT("Topology %s: Duplicate stargate name: %s."), pNode->GetID(), sGateName);
				::kernelDebugLogString(sWarning);
				}
			}

		//	For other cases we no longer handle [Prev] stargates because the code
		//	now automatically creates return gates.

		return NOERROR;
		}

	//	Figure out where the stargate goes

	CTopologyNode *pDest = NULL;

	//	If we're looking for the fragment exit, replace with appropriate gate

	if (strEquals(sDest, FRAGMENT_EXIT_DEST))
		{
		sDest = Ctx.sFragmentExitID;
		sDestEntryPoint = Ctx.sFragmentExitGate;
		}

	//	Get the node
	//
	//	NOTE: The node returned might not have the same ID as sDest (because we might
	//	have hit a table that picks a random node).

	if (error = GetOrAddTopologyNode(Ctx, sDest, pNode, pGateDesc, &pDest))
		return error;

	//	If we don't have a node, then ignore (this can happen if we point to a node descriptor
	//	that is empty).

	if (pDest == NULL)
		return NOERROR;

	//	Prepare the route descriptor

	CTopologyNode::SStargateRouteDesc RouteDesc;
	RouteDesc.pFromNode = pNode;
	RouteDesc.sFromName = sGateName;
	RouteDesc.pToNode = pDest;
	RouteDesc.sToName = sDestEntryPoint;
	RouteDesc.bOneWay = bOneWay;

	//	Collect any stargate options

	CString sPath;
	if (pGateDesc->FindAttribute(PATH_ATTRIB, &sPath))
		{
		if (error = CTopologyNode::ParsePointList(sPath, &RouteDesc.MidPoints))
			{
			Ctx.sError = CONSTLIT("Invalid stargate path points.");
			return error;
			}
		}

	//	Create

	if (error = CTopologyNode::CreateStargateRoute(RouteDesc))
		{
		//	A duplicate name is not an error--just a warning

		CString sWarning = strPatternSubst(CONSTLIT("Topology %s: Duplicate stargate name: %s."), pNode->GetID(), sGateName);
		::kernelDebugLogString(sWarning);
		}

	return NOERROR;
	}

ALERROR CTopology::AddTopology (STopologyCreateCtx &Ctx)

//	AddTopology
//
//	Adds a new topology.

	{
	ALERROR error;
	int i;

	//	We only worry about the first table.

	CTopologyDescTable *pTable = (Ctx.Tables.GetCount() > 0 ? Ctx.Tables[0] : NULL);
	if (pTable == NULL)
		return NOERROR;

	//	Add topology starting at each root node.

	for (i = 0; i < pTable->GetRootNodeCount(); i++)
		{
		CTopologyDesc *pNodeDesc = pTable->GetRootNodeDesc(i);

		//	Make sure this root node hasn't already been added. If it has, it
		//	just means that we've linked from one topology to the root node of
		//	another. This is not an error.

		if (FindTopologyNode(pNodeDesc->GetID()))
			continue;

		//	Add the node and recursively add all nodes leading out of
		//	the root node.

		if (error = AddTopologyDesc(Ctx, pNodeDesc))
			return error;
		}

	return NOERROR;
	}

ALERROR CTopology::AddTopologyDesc (STopologyCreateCtx &Ctx, CTopologyDesc *pNode, CTopologyNode **retpNewNode)

//	AddTopologyDesc
//
//	This is a recursive function that adds all nodes for the given node descriptor.

	{
	//	Is this an EndGame node?

	CString sEpitaph;
	CString sReason;
	if (pNode->IsEndGameNode(&sEpitaph, &sReason))
		{
		CString sID = pNode->GetID();
		CString sFullID = ExpandNodeID(Ctx, sID);

		//	Create a topology node and add it to the universe list

		CTopologyNode *pNewNode = new CTopologyNode(sFullID, END_GAME_SYSTEM_UNID, Ctx.pMap);
		AddTopologyNode(sFullID, pNewNode);
		if (Ctx.pNodesAdded)
			Ctx.pNodesAdded->Insert(pNewNode);

		//	Load stuff about the end game

		pNewNode->SetEpitaph(sEpitaph);
		pNewNode->SetEndGameReason(sReason);

		//	We're done

		if (retpNewNode)
			*retpNewNode = pNewNode;

		return NOERROR;
		}

	//	If this is a NodeGroup, then redirect

	else if (pNode->GetType() == ndNodeGroup)
		return AddNodeGroup(Ctx, pNode, retpNewNode);

	//	If this is a NodeTable, then redirect

	else if (pNode->GetType() == ndNodeTable)
		return AddNodeTable(Ctx, pNode, retpNewNode);

	//	If this is a Fragment, then add it

	else if (pNode->GetType() == ndFragment)
		return AddFragment(Ctx, pNode, retpNewNode);

	//	If this is a Network, then add it

	else if (pNode->GetType() == ndNetwork)
		return AddNetwork(Ctx, pNode, retpNewNode);

	//	If this is a Random fragment, then add it

	else if (pNode->GetType() == ndRandom)
		return AddRandom(Ctx, pNode, retpNewNode);

	//	Otherwise, add a single node and any nodes that lead away from it

	else if (pNode->GetType() == ndNode)
		return AddNode(Ctx, pNode, retpNewNode);

	//	Error

	else
		{
		ASSERT(false);
		return ERR_FAIL;
		}
	}

ALERROR CTopology::AddTopologyNode (const CString &sID, CTopologyNode *pNode)

//	AddTopologyNode
//
//	Adds the node to the topology

	{
	int iPos = m_Topology.GetCount();
	m_Topology.Insert(pNode);
	m_IDToNode.Insert(sID, iPos);
	return NOERROR;
	}

ALERROR CTopology::AddTopologyNode (STopologyCreateCtx &Ctx, const CString &sNodeID, CTopologyNode **retpNewNode)

//	AddTopologyNode
//
//	Adds a topology node by ID or returns the node if it already exists.
//
//	NOTE: In some cases this function can return NOERROR and a NULL node.

	{
	return GetOrAddTopologyNode(Ctx, sNodeID, NULL, NULL, retpNewNode);
	}

ALERROR CTopology::ApplyNodeTemplate (STopologyCreateCtx &Ctx, const TArray<CTopologyNode *> &Nodes, CXMLElement *pNodeTemplate, bool bUnmarkedOnly)

//	ApplyNodeTemplate
//
//	Applies the settings in the node template to the given list of nodes.

	{
	ALERROR error;
	int i, j;

	CString sNodeAttribs = pNodeTemplate->GetAttribute(ATTRIBUTES_ATTRIB);

	//	If we have a <System> element, we use it to initialize data

	CXMLElement *pSystemXML = pNodeTemplate->GetContentElementByTag(SYSTEM_TAG);

	//	If the <System> element has a child, then we assume it is a generator

	TUniquePtr<IElementGenerator> pGenerator;
	if (pSystemXML && pSystemXML->GetContentElementCount() > 0)
		{
		SDesignLoadCtx LoadCtx;

		if (error = IElementGenerator::CreateFromXMLAsGroup(LoadCtx, pSystemXML, pGenerator))
			{
			Ctx.sError = LoadCtx.sError;
			return error;
			}
		}

	//	Loop over all nodes and apply

	for (i = 0; i < Nodes.GetCount(); i++)
		{
		CTopologyNode *pNode = Nodes[i];

		//	Skip marked nodes, if necessary

		if (bUnmarkedOnly && pNode->IsMarked())
			continue;

		//	Apply attributes

		pNode->AddAttributes(sNodeAttribs);

		//	Apply root system (but ignore any sub-elements of pSystemXML, because we
		//	will apply the separately with the generator).

		if (pSystemXML)
			{
			if (error = pNode->InitFromSystemXML(*this, pSystemXML, &Ctx.sError, true))
				return error;
			}

		//	Apply the generator

		if (pGenerator)
			{
			IElementGenerator::SCtx GenCtx;
			GenCtx.pTopology = this;

			TArray<IElementGenerator::SResult> Result;
			pGenerator->Generate(GenCtx, Result);

			for (j = 0; j < Result.GetCount(); j++)
				{
				if (error = pNode->InitFromSystemXML(*this, Result[j].pElement, &Ctx.sError))
					return error;
				}
			}
		}

	//	Done

	return NOERROR;
	}

ALERROR CTopology::CreateTopologyNode (STopologyCreateCtx &Ctx, const CString &sID, SNodeCreateCtx &NodeCtx, CTopologyNode **retpNode)

//	CreateTopologyNode
//
//	Creates and adds a single topology node. We retain ownership of the node, but
//	optionally return a pointer to it.

	{
	ALERROR error;

	CSystemMap *pDestMap = (NodeCtx.bNoMap ? NULL : Ctx.pMap);

	//	Create a topology node and add it to the universe list

	CTopologyNode *pNewNode = new CTopologyNode(sID, 0, pDestMap);
	AddTopologyNode(sID, pNewNode);
	if (Ctx.pNodesAdded)
		Ctx.pNodesAdded->Insert(pNewNode);

	//	Map position

	int xPos;
	int yPos;
	int iRotation;
	GetAbsoluteDisplayPos(Ctx, NodeCtx.xPos, NodeCtx.yPos, &xPos, &yPos, &iRotation);
	pNewNode->SetPos(xPos, yPos);

	//	Add attributes for the node

	pNewNode->AddAttributes(NodeCtx.sAttribs);
	if (Ctx.bInFragment && !Ctx.sFragmentAttributes.IsBlank())
		pNewNode->AddAttributes(Ctx.sFragmentAttributes);

	//	Get the system tag and apply settings

	if (NodeCtx.pSystemDesc)
		{
		if (error = pNewNode->InitFromSystemXML(*this, NodeCtx.pSystemDesc, &Ctx.sError))
			return error;
		}

	//	Add effect associated with node

	if (pDestMap && NodeCtx.pEffect)
		pDestMap->AddAnnotation(sID, NodeCtx.pEffect, xPos, yPos, iRotation);

	//	Set the initial player state

	switch (NodeCtx.iInitialState)
		{
		case CTopologyDesc::statePositionKnown:
			pNewNode->SetPositionKnown();
			break;

		case CTopologyDesc::stateKnown:
			//	NOTE: This implies that the position is known, so no need to set
			//	it separately.
			pNewNode->SetKnown();
			break;
		}

	//	Done

	if (retpNode)
		*retpNode = pNewNode;

	return NOERROR;
	}

void CTopology::DeleteAll (void)

//	DeleteAll
//
//	Delete all nodes

	{
	int i;

	for (i = 0; i < GetTopologyNodeCount(); i++)
		{
		CTopologyNode *pNode = GetTopologyNode(i);
		delete pNode;
		}

	m_Topology.DeleteAll();
	m_IDToNode.DeleteAll();
	}

CString CTopology::ExpandNodeID (STopologyCreateCtx &Ctx, const CString &sID)

//	ExpandNodeID
//
//	Expands a Fragment-relative ID to a full ID

	{
	if (!Ctx.sFragmentPrefix.IsBlank() && *sID.GetASCIIZPointer() == '+')
		return strPatternSubst(CONSTLIT("%s%s"), Ctx.sFragmentPrefix, sID);
	else
		return sID;
	}

CTopologyDesc *FindNodeInContext (STopologyCreateCtx &Ctx, const CString &sNodeID)
	{
	int i;

	for (i = 0; i < Ctx.Tables.GetCount(); i++)
		{
		CTopologyDesc *pNode = Ctx.Tables[i]->FindTopologyDesc(sNodeID);
		if (pNode)
			return pNode;
		}

	return NULL;
	}

ALERROR CTopology::FindTopologyDesc (STopologyCreateCtx &Ctx, const CString &sNodeID, CTopologyDesc **retpNode, NodeTypes *retiNodeType)

//	FindTopologyDesc
//
//	Looks for the node desc by name both in our own topology and in the context

	{
	CTopologyDesc *pDestNode;

	//	If we're in a fragment, look for the node first in the
	//	fragment table (this is only an optimization)

	if (Ctx.bInFragment
			&& Ctx.pFragmentTable 
			&& (pDestNode = Ctx.pFragmentTable->FindTopologyDesc(sNodeID))
			&& !pDestNode->IsRootNode())
		{
		*retpNode = pDestNode;
		if (retiNodeType)
			*retiNodeType = typeFragment;
		return NOERROR;
		}

	//	Otherwise, look for the destination node in our topology.
	//	If we found it, add it (recursively).

	else if (pDestNode = FindNodeInContext(Ctx, sNodeID))
		{
		*retpNode = pDestNode;
		if (retiNodeType)
			*retiNodeType = typeStandard;
		return NOERROR;
		}

	//	Otherwise, see if we find the node in our fragment table.
	//	If found, then we add the fragment to the list

	else if (Ctx.pFragmentTable && (pDestNode = Ctx.pFragmentTable->FindRootNodeDesc(sNodeID)))
		{
		*retpNode = pDestNode;
		if (retiNodeType)
			*retiNodeType = typeFragmentStart;
		return NOERROR;
		}

	//	Otherwise, we can't find the destination

	else
		{
		Ctx.sError = strPatternSubst(CONSTLIT("NodeID not found: %s"), sNodeID);
		return ERR_FAIL;
		}
	}

bool CTopology::FindNearestNodeCreatedBy (const CString &sID, CTopologyNode *pNode, CTopologyNode **retpNewNode) const

//	FindNearestNodeCreatedBy
//
//	Finds the nearest node created by sID to pNode. Returns FALSE if we could not
//	find such a node.

	{
	int i;

	int xTarget, yTarget;
	CSystemMap *pTargetMap = pNode->GetDisplayPos(&xTarget, &yTarget);

	CTopologyNode *pBestNode = NULL;
	Metric rBestDist2 = 0.0;
	for (i = 0; i < GetTopologyNodeCount(); i++)
		{
		CTopologyNode *pTestNode = GetTopologyNode(i);
		int xNode, yNode;
		CSystemMap *pMap = pTestNode->GetDisplayPos(&xNode, &yNode);

		if (pMap == pTargetMap
				&& strEquals(pTestNode->GetCreatorID(), sID))
			{
			//	If we don't care about the specific node, we're done.

			if (retpNewNode == NULL)
				return true;

			//	Otherwise, see if this node is closer than the previous one.

			int xDist = (xNode - xTarget);
			int yDist = (yNode - yTarget);
			Metric rDist2 = (xDist * xDist) + (yDist * yDist);
			if (pBestNode == NULL || rDist2 < rBestDist2)
				{
				pBestNode = pTestNode;
				rBestDist2 = rDist2;
				}
			}
		}

	//	Done

	if (pBestNode == NULL)
		return false;

	*retpNewNode = pBestNode;
	return true;
	}

CTopologyNode *CTopology::FindTopologyNode (const CString &sID) const

//	FindTopologyNode
//
//	Look for the given node

	{
	int *pPos = m_IDToNode.GetAt(sID);
	if (pPos == NULL)
		return NULL;

	return m_Topology.GetAt(*pPos);
	}

CString CTopology::GenerateUniquePrefix (const CString &sPrefix, const CString &sTestNodeID)

//	GenerateUniquePrefix
//
//	Returns a unique prefix based on sPrefix and using sTestNodeID to ensure uniqueness

	{
	CString sTestPrefix = sPrefix;
	int iNumber = 2;
	while (FindTopologyNode(strPatternSubst(CONSTLIT("%s%s"), sTestPrefix, sTestNodeID)))
		sTestPrefix = strPatternSubst(CONSTLIT("%s%d"), sPrefix, iNumber++);

	return sTestPrefix;
	}

void CTopology::GetAbsoluteDisplayPos (STopologyCreateCtx &Ctx, int x, int y, int *retx, int *rety, int *retiRotation)

//	GetAbsoluteDisplayPos
//
//	Converts to absolute coordinates if we're in a fragment

	{
	if (Ctx.bInFragment)
		{
		*retiRotation = Ctx.iRotation;
		if (*retiRotation != 0)
			{
			CVector vPos = CVector(x, y).Rotate(*retiRotation);
			x = (int)(vPos.GetX() + 0.5);
			y = (int)(vPos.GetY() + 0.5);
			}

		*retx = x + Ctx.xOffset;
		*rety = y + Ctx.yOffset;
		}
	else
		{
		*retiRotation = 0;
		*retx = x;
		*rety = y;
		}
	}

int CTopology::GetDistance (const CString &sSourceID, const CString &sDestID) const

//	GetDistance
//
//	Returns the shortest distance between the two nodes. If there is no path between
//	the two nodes, we return -1.

	{
	int i;

	if (GetTopologyNodeCount() < 2)
		return -1;

	//	We mark nodes to track our progress, so we have to save the
	//	previous value of the marks.
	//
	//	We also initialize the calculated distance to UNKNOWN_DISTANCE

	bool *pOldMarks = new bool [GetTopologyNodeCount()];
	for (i = 0; i < GetTopologyNodeCount(); i++)
		{
		pOldMarks[i] = GetTopologyNode(i)->IsMarked();
		GetTopologyNode(i)->SetMarked(false);
		GetTopologyNode(i)->SetCalcDistance(UNKNOWN_DISTANCE);
		}

	//	Find the source node in the list

	CTopologyNode *pSource = FindTopologyNode(sSourceID);
	if (pSource == NULL)
		return -1;

	//	Find the destination node; We know that it's distance is 0

	CTopologyNode *pDest = FindTopologyNode(sDestID);
	if (pDest == NULL)
		return -1;

	pDest->SetCalcDistance(0);

	//	Loop over all gates and recurse

	int iBestDist = GetDistance(pSource, sDestID, INFINITE_DISTANCE);

	//	Restore

	for (i = 0; i < GetTopologyNodeCount(); i++)
		GetTopologyNode(i)->SetMarked(pOldMarks[i]);
	delete pOldMarks;

	//	Done

	return (iBestDist != INFINITE_DISTANCE ? iBestDist : -1);
	}

int CTopology::GetDistance (CTopologyNode *pSource, const CString &sDestID, int iBestDist) const

//	GetDistance
//
//	Returns the distance from pSource to sDestID, using calculated distance
//	(stored in each topology node).
//
//	NOTE: We mark nodes that we are in the middle of evaluating, so that we don't
//	recurse infinitely. Callers must clear all marks.

	{
	int i;

	//	If we already know the distance of this node, then we return it.

	int iNewBestDist = pSource->GetCalcDistance();
	if (iNewBestDist != UNKNOWN_DISTANCE)
		return iNewBestDist;

	//	If the best distance that we've already calculated is 0, then we fail
	//	(since we can't get closer than that). This optimizes the algorithm
	//	so we don't keep evaluating paths that are longer than the best path.

	if (iBestDist == 0)
		return INFINITE_DISTANCE;

	//	Mark this node so that we don't recurse into it.

	bool bSavedMark = pSource->IsMarked();
	pSource->SetMarked();

	//	Recursively compute the distance based on any neighboring
	//	systems.

	bool bDistKnown = true;
	iNewBestDist = INFINITE_DISTANCE;
	for (i = 0; i < pSource->GetStargateCount(); i++)
		{
		CTopologyNode *pDest = pSource->GetStargateDest(i);
		if (pDest == NULL)
			continue;

		//	Try to calculate the distance, if we can

		if (!pDest->IsMarked())
			{
			int iDist = GetDistance(pDest, sDestID, iNewBestDist - 1);
			if (iDist < iNewBestDist)
				{
				iNewBestDist = iDist;

				//	Can't get closer than this, so we're done.

				if (iNewBestDist == 0)
					break;
				}
			}

		//	If the destination node doesn't know its distance, then we can't
		//	compute our distance either (it means that we're down a dead-end branch
		//	or something).

		if (pDest->GetCalcDistance() == UNKNOWN_DISTANCE)
			bDistKnown = false;
		}

	//	Clear the mark

	pSource->SetMarked(bSavedMark);

	//	Our distance is 1 more than the best distance from any of our neighbors.

	if (iNewBestDist != INFINITE_DISTANCE)
		iNewBestDist++;

	//	If we have enough information to set our distance, do it now

	if (bDistKnown)
		pSource->SetCalcDistance(iNewBestDist);

	//	Done

	return iNewBestDist;
	}

ALERROR CTopology::GetOrAddTopologyNode (STopologyCreateCtx &Ctx, 
										 const CString &sID, 
										 CTopologyNode *pPrevNode, 
										 CXMLElement *pGateDesc, 
										 CTopologyNode **retpNode)

//	GetOrAddTopologyNode
//
//	Looks for the topology node by ID and returns it. If it is not found, it adds it
//	(And recursively adds other nodes that it points to).
//
//	NOTE: There are cases where the ID of the node created is different from the
//	requested ID (this can happen, e.g., if we're adding a fragment).
//
//	NOTE: In some cases this function can return NOERROR and a NULL Node.

	{
	ALERROR error;

	CString sFullID = ExpandNodeID(Ctx, sID);

	//	See if the node has already been created. If so, return it.

	CTopologyNode *pNode = FindTopologyNode(sFullID);
	if (pNode)
		{
		if (retpNode)
			*retpNode = pNode;

		return NOERROR;
		}

	//	Look for the descriptor.

	CTopologyDesc *pDestDesc;
	NodeTypes iDestNodeType;
	if (error = FindTopologyDesc(Ctx, sID, &pDestDesc, &iDestNodeType))
		return error;

	//	Add the descriptor

	STopologyCreateCtx NewCtx = Ctx;
	NewCtx.pPrevNode = pPrevNode;

	//	Set bInFragment to TRUE if we are adding a node that is in a fragment (either because we're
	//	starting a new fragment or because we are getting the next node in a fragment).

	NewCtx.bInFragment = (pGateDesc && (iDestNodeType == typeFragment || iDestNodeType == typeFragmentStart));

	//	If we're not continuing within a fragment, then initialize the fragment parameters.
	//	(If we're continuing, then we just keep the existing fragment info).

	if (pGateDesc && iDestNodeType != typeFragment)
		{
		//	If the gate specifies a fragment rotation, then we assume that the destination is
		//	a fragment. [Note: This means that we always have to have a destFragmentRotation attribute
		//	to call a fragment.]

		int iRotation = pGateDesc->GetAttributeIntegerBounded(DEST_FRAGMENT_ROTATION_ATTRIB, 0, 359, -1);
		if (iRotation != -1)
			{
			NewCtx.bInFragment = true;

			NewCtx.iRotation = iRotation;
			pPrevNode->GetDisplayPos(&NewCtx.xOffset, &NewCtx.yOffset);
			NewCtx.sFragmentAttributes = pGateDesc->GetAttribute(DEST_FRAGMENT_ATTRIBUTES_ATTRIB);
			CTopologyNode::ParseStargateString(pGateDesc->GetAttribute(DEST_FRAGMENT_EXIT_ATTRIB), &NewCtx.sFragmentExitID, &NewCtx.sFragmentExitGate);
			}
		else
			{
			NewCtx.iRotation = 0;
			NewCtx.sFragmentAttributes = NULL_STR;
			NewCtx.sFragmentExitID = NULL_STR;
			NewCtx.sFragmentExitGate = NULL_STR;
			}
		}

	//	Add the nodes (NOTE: pNode can come back NULL).

	if (error = AddTopologyDesc(NewCtx, pDestDesc, &pNode))
		{
		Ctx.sError = NewCtx.sError;
		return error;
		}

	//	Done

	if (retpNode)
		*retpNode = pNode;

	return NOERROR;
	}

void CTopology::GetFragmentDisplayPos (STopologyCreateCtx &Ctx, CTopologyNode *pNode, int *retx, int *rety)

//	GetFragmentDisplayPos
//
//	Returns the position of the given node in fragment coordinates

	{
	int x, y;
	pNode->GetDisplayPos(&x, &y);

	//	Convert to fragment coordinates

	if (Ctx.bInFragment)
		{
		x -= Ctx.xOffset;
		y -= Ctx.yOffset;

		if (Ctx.iRotation != 0)
			{
			CVector vPos = CVector(x, y).Rotate(360 - Ctx.iRotation);
			x = (int)(vPos.GetX() + 0.5);
			y = (int)(vPos.GetY() + 0.5);
			}
		}

	//	Done

	*retx = x;
	*rety = y;
	}

void CTopology::GetFragmentEntranceDisplayPos (STopologyCreateCtx &Ctx, int *retx, int *rety) const

//	GetFragmentEntranceDisplayPos
//
//	Returns the position of the entrance to the fragment in the appropriate
//	coordinate.

	{
	//	For a true fragment, which uses coordinates local to the fragment, the
	//	entrance is always at 0,0.

	if (Ctx.bInFragment)
		{
		*retx = 0;
		*rety = 0;
		}

	//	Otherwise, we use the previous node coordinates

	else if (Ctx.pPrevNode)
		{
		Ctx.pPrevNode->GetDisplayPos(retx, rety);
		}

	//	Otherwise, we don't know what to do, so we set to 0,0.

	else
		{
		*retx = 0;
		*rety = 0;
		}
	}

ALERROR CTopology::InitComplexArea (CXMLElement *pAreaDef, int iMinRadius, CComplexArea *retArea, STopologyCreateCtx *pCtx, CTopologyNode **iopExit)

//	InitComplexArea
//
//	Initializes the complex area structure from and XML definition.
//
//	If pCtx and iopExit are passed in, then we interpret the "FragmentEntrance" and "FragmentExit" positions

	{
	ALERROR error;
	int i;

	//	We allow embeded <Table> and <Group> elements.
	//	Generate the results.

	IElementGenerator::SCtx GenCtx;
	GenCtx.pTopology = this;

	TArray<CXMLElement *> AreaDef;
	CString sError;

	if (!IElementGenerator::GenerateAsGroup(GenCtx, pAreaDef, AreaDef, &sError))
		{
		if (pCtx)
			pCtx->sError = strPatternSubst(CONSTLIT("Unable to generate <Area> definitions table: %s"), sError);
		return ERR_FAIL;
		}

	bool bUsesFragmentEntrance = false;

	for (i = 0; i < AreaDef.GetCount(); i++)
		{
		CXMLElement *pElement = AreaDef[i];

		if (strEquals(pElement->GetTag(), LINE_TAG))
			{
			int xFrom, yFrom;
			if (error = AddRandomParsePosition(pCtx, pElement->GetAttribute(FROM_ATTRIB), iopExit, &xFrom, &yFrom))
				return error;

			int xTo, yTo;
			if (error = AddRandomParsePosition(pCtx, pElement->GetAttribute(TO_ATTRIB), iopExit, &xTo, &yTo))
				return error;

			int iRadius = pElement->GetAttributeIntegerBounded(RADIUS_ATTRIB, 1, -1, iMinRadius);

			//	If we refer to [FragmentEntrance] then we should exclude the area
			//	around it, even if not in a real fragment.

			if (pCtx
					&& !bUsesFragmentEntrance
					&& strEquals(pElement->GetAttribute(FROM_ATTRIB), FRAGMENT_ENTRANCE_DEST))
				bUsesFragmentEntrance = true;

			//	Add two circles around the end points

			retArea->IncludeCircle(xFrom, yFrom, iRadius);
			retArea->IncludeCircle(xTo, yTo, iRadius);

			//	Add the rect between them

			int iLength;
			int iRotation = IntVectorToPolar(xTo - xFrom, yTo - yFrom, &iLength);
			int xLL, yLL;
			IntPolarToVector(270 + iRotation, iRadius, &xLL, &yLL);

			retArea->IncludeRect(xFrom + xLL, yFrom + yLL, iLength, 2 * iRadius, iRotation);
			}
		else
			{
			if (pCtx)
				pCtx->sError = strPatternSubst(CONSTLIT("Unknown <Area> subelement: %s"), pElement->GetTag());
			return ERR_FAIL;
			}
		}

	//	Exclude some areas around the entrance/exit


	if (pCtx)
		{
		//	If we're part of a fragment, then exclude the region around the entrance and exit nodes

		if (pCtx->bInFragment)
			retArea->ExcludeCircle(0, 0, iMinRadius);

		//	If we've referred to the fragment entrance, then exclude it, even if
		//	we're not a true fragment.

		else if (bUsesFragmentEntrance && pCtx->pPrevNode)
			{
			int xEntrance, yEntrance;
			pCtx->pPrevNode->GetDisplayPos(&xEntrance, &yEntrance);
			retArea->ExcludeCircle(xEntrance, yEntrance, iMinRadius);
			}

		//	Exclude the exit

		if (pCtx->bInFragment && *iopExit)
			{
			int xExit, yExit;
			GetFragmentDisplayPos(*pCtx, *iopExit, &xExit, &yExit);

			retArea->ExcludeCircle(xExit, yExit, iMinRadius);
			}
		}

	return NOERROR;
	}

void CTopology::ReadFromStream (SUniverseLoadCtx &Ctx)

//	ReadFromStream
//
//	Read the topology

	{
	int i;
	DWORD dwCount;

	Ctx.pStream->Read((char *)&dwCount, sizeof(DWORD));
	for (i = 0; i < (int)dwCount; i++)
		{
		CTopologyNode *pNode;
		CTopologyNode::CreateFromStream(Ctx, &pNode);

		AddTopologyNode(pNode->GetID(), pNode);
		}
	}

//	Initialize Topology

ALERROR CUniverse::InitTopology (DWORD dwStartingMap, CString *retsError)

//	InitTopology
//
//	Initializes the star system topology

	{
	ALERROR error;
	int i;

	//	No need to initialize if we've already loaded it

	if (m_Topology.GetTopologyNodeCount() > 0)
		return NOERROR;

	//	Add all root nodes in order. AddTopologyDesc will recurse and
	//	add all nodes connected to each root.
	//
	//	This is here for backwards compatibility with the old
	//	<SystemTopology> element.

	CTopologyDescTable *pTopology = m_Design.GetTopologyDesc();
	if (pTopology)
		{
		STopologyCreateCtx Ctx;
		Ctx.Tables.Insert(pTopology);

		if (error = m_Topology.AddTopology(Ctx))
			{
			*retsError = Ctx.sError;
			return error;
			}
		}

	//	Keep track of nodes added by display map, so that we can run the 
	//	topology processors on them later.

	TSortMap<DWORD, CTopologyNodeList> NodesAdded;
	TArray<CSystemMap *> PrimaryMaps;
	TArray<CSystemMap *> SecondaryMaps;

	//	Let the maps add their topologies

	for (i = 0; i < m_Design.GetCount(designSystemMap); i++)
		{
		CSystemMap *pMap = CSystemMap::AsType(m_Design.GetEntry(designSystemMap, i));

		//	If this is a starting map and it is not the map that we're starting
		//	with, then skip it.

		if (pMap->IsStartingMap() && pMap->GetUNID() != dwStartingMap)
			continue;

		//	Add topology

		if (error = pMap->AddFixedTopology(m_Topology, NodesAdded, retsError))
			return error;

		//	Keep track of primary vs. secondary maps

		if (pMap->IsPrimaryMap())
			PrimaryMaps.Insert(pMap);
		else
			SecondaryMaps.Insert(pMap);
		}

	//	Run all primary map topology processors. We guarantee that the topology 
	//	processors of a primary map will run before any secondary ones.

	for (i = 0; i < PrimaryMaps.GetCount(); i++)
		{
		CSystemMap *pMap = PrimaryMaps[i];

		//	Get the set of nodes that this map operates on.

		CTopologyNodeList *pNodeList = NodesAdded.GetAt(pMap->GetUNID());
		if (pNodeList == NULL || pNodeList->GetCount() == 0)
			continue;

		//	Process topology

		if (error = pMap->ProcessTopology(m_Topology, pMap, *pNodeList, retsError))
			return error;
		}

	//	Now run all secondary map topology processors

	for (i = 0; i < SecondaryMaps.GetCount(); i++)
		{
		CSystemMap *pMap = SecondaryMaps[i];

		//	Secondary maps operate on nodes in a primary map.

		CSystemMap *pTargetMap = pMap->GetDisplayMap();
		CTopologyNodeList *pNodeList = NodesAdded.GetAt(pTargetMap->GetUNID());
		if (pNodeList == NULL || pNodeList->GetCount() == 0)
			continue;

		//	Process topology

		if (error = pMap->ProcessTopology(m_Topology, pTargetMap, *pNodeList, retsError))
			return error;
		}

	//	Make sure every node added has a system UNID

	for (i = 0; i < PrimaryMaps.GetCount(); i++)
		{
		CSystemMap *pMap = PrimaryMaps[i];

		//	Get the set of nodes that this map operates on.

		CTopologyNodeList *pNodeList = NodesAdded.GetAt(pMap->GetUNID());
		if (pNodeList == NULL || pNodeList->GetCount() == 0)
			continue;

		//	Make sure each node has a system type defined

		for (int j = 0; j < pNodeList->GetCount(); j++)
			if (pNodeList->GetAt(j)->GetSystemTypeUNID() == 0)
				{
				*retsError = strPatternSubst(CONSTLIT("SystemMap (%x): NodeID %s: No system specified"), pMap->GetUNID(), pNodeList->GetAt(j)->GetID());
				return ERR_FAIL;
				}
		}

	//	Add the topologies from any enabled extensions
	//
	//	This is here for compatibility with the old <SystemTopology> elements.

	for (i = 0; i < m_Design.GetExtensionCount(); i++)
		{
		CExtension *pExtension = m_Design.GetExtension(i);
		if (pExtension->GetTopology().GetRootNodeCount() > 0
				&& pExtension->GetType() != extAdventure)
			{
			STopologyCreateCtx Ctx;
			Ctx.Tables.Insert(&pExtension->GetTopology());

			if (error = m_Topology.AddTopology(Ctx))
				{
				*retsError = Ctx.sError;
				return error;
				}
			}
		}

	//	If we've got no topology nodes then that's an error

	if (m_Topology.GetTopologyNodeCount() == 0)
		{
		*retsError = CONSTLIT("No topology nodes found");
		return ERR_FAIL;
		}

	//	Call OnGlobalTopologyCreated

	for (i = 0; i < m_Design.GetCount(); i++)
		if (error = m_Design.GetEntry(i)->FireOnGlobalTopologyCreated(retsError))
			return error;

	//	For all encounters that are required (i.e., specify an exact number to 
	//	be encountered) we distribute them randomly across all topology nodes.

	if (error = InitRequiredEncounters(retsError))
		return error;

	return NOERROR;
	}


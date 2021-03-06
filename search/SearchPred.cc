// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "Machine.hh"
#include "TimingArc.hh"
#include "TimingRole.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Sdc.hh"
#include "Levelize.hh"
#include "Search.hh"
#include "Latches.hh"
#include "SearchPred.hh"

namespace sta {

static bool
searchThruSimEdge(const Vertex *vertex, const TransRiseFall *tr);
static bool
searchThruTimingSense(const Edge *edge, const TransRiseFall *from_tr,
		      const TransRiseFall *to_tr);

SearchPred0::SearchPred0(const StaState *sta) :
  sta_(sta)
{
}

bool
SearchPred0::searchFrom(const Vertex *from_vertex)
{
  return !(from_vertex->isDisabledConstraint()
	   || from_vertex->isConstant());
}

bool
SearchPred0::searchThru(Edge *edge)
{
  const TimingRole *role = edge->role();
  const Sdc *sdc = sta_->sdc();
  return !(edge->isDisabledConstraint()
	   // Constants disable edge cond expression.
	   || edge->isDisabledCond()
	   || sdc->isDisabledCondDefault(edge)
	   // Register/latch preset/clr edges are disabled by default.
	   || (role == TimingRole::regSetClr()
	       && !sdc->presetClrArcsEnabled())
	   // Constants on other pins disable this edge (ie, a mux select).
	   || edge->simTimingSense() == timing_sense_none
	   || (edge->isBidirectInstPath()
	       && !sdc->bidirectInstPathsEnabled())
	   || (edge->isBidirectNetPath()
	       && !sdc->bidirectNetPathsEnabled())
	   || (role == TimingRole::latchDtoQ()
	       && sta_->latches()->latchDtoQState(edge)==latch_state_closed));
}

bool
SearchPred0::searchTo(const Vertex *to_vertex)
{
  return !to_vertex->isConstant();
}

////////////////////////////////////////////////////////////////

SearchPred1::SearchPred1(const StaState *sta) :
  SearchPred0(sta)
{
}

bool
SearchPred1::searchThru(Edge *edge)
{
  return SearchPred0::searchThru(edge)
    && !edge->isDisabledLoop();
}

////////////////////////////////////////////////////////////////

SearchPred2::SearchPred2(const StaState *sta) :
  SearchPred1(sta)
{
}

bool
SearchPred2::searchThru(Edge *edge)
{
  return SearchPred1::searchThru(edge)
    && !edge->role()->isTimingCheck();
}

////////////////////////////////////////////////////////////////

SearchPredNonLatch2::SearchPredNonLatch2(const StaState *sta) :
  SearchPred2(sta)
{
}

bool
SearchPredNonLatch2::searchThru(Edge *edge)
{
  return SearchPred2::searchThru(edge)
    && !sta_->latches()->isLatchDtoQ(edge);
}

////////////////////////////////////////////////////////////////

SearchPredNonReg2::SearchPredNonReg2(const StaState *sta) :
  SearchPred2(sta)
{
}

bool
SearchPredNonReg2::searchThru(Edge *edge)
{
  const TimingRole *role = edge->role();
  return SearchPred2::searchThru(edge)
    // Enqueue thru latches is handled explicitly by search.
    && !sta_->latches()->isLatchDtoQ(edge)
    && role->genericRole() != TimingRole::regClkToQ();
}

////////////////////////////////////////////////////////////////

ClkTreeSearchPred::ClkTreeSearchPred(const StaState *sta) :
  SearchPred1(sta)
{
}

bool
ClkTreeSearchPred::searchThru(Edge *edge)
{
  const Sdc *sdc = sta_->sdc();
  // Propagate clocks through constants.
  const TimingRole *role = edge->role();
  return (role->isWire()
	  || role == TimingRole::combinational())
    && (sdc->clkThruTristateEnabled()
	|| !(role == TimingRole::tristateEnable()
	     || role == TimingRole::tristateDisable()))
    && SearchPred1::searchThru(edge);
}

bool
isClkEnd(Vertex *vertex,
	 Graph *graph)
{
  ClkTreeSearchPred pred(graph);
  VertexOutEdgeIterator edge_iter(vertex, graph);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    if (pred.searchThru(edge))
      return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////

bool
searchThru(const Edge *edge,
	   const TimingArc *arc,
	   const Graph *graph)
{
  TransRiseFall *from_tr = arc->fromTrans()->asRiseFall();
  TransRiseFall *to_tr = arc->toTrans()->asRiseFall();
  // Ignore transitions other than rise/fall.
  return from_tr && to_tr
    && searchThru(edge->from(graph), from_tr, edge, edge->to(graph), to_tr);
}

bool
searchThru(Vertex *from_vertex,
	   const TransRiseFall *from_tr,
	   const Edge *edge,
	   Vertex *to_vertex,
	   const TransRiseFall *to_tr)
{
  return searchThruTimingSense(edge, from_tr, to_tr)
    && searchThruSimEdge(from_vertex, from_tr)
    && searchThruSimEdge(to_vertex, to_tr);
}

// set_case_analysis rising/falling filters rise/fall edges during search.
static bool
searchThruSimEdge(const Vertex *vertex, const TransRiseFall *tr)
{
  LogicValue sim_value = vertex->simValue();
  switch (sim_value) {
  case logic_rise:
    return tr == TransRiseFall::rise();
  case logic_fall:
    return tr == TransRiseFall::fall();
  default:
    return true;
  };
}

static bool
searchThruTimingSense(const Edge *edge, const TransRiseFall *from_tr,
		      const TransRiseFall *to_tr)
{
  switch (edge->simTimingSense()) {
  case timing_sense_unknown:
    return true;
  case timing_sense_positive_unate:
    return from_tr == to_tr;
  case timing_sense_negative_unate:
    return from_tr != to_tr;
  case timing_sense_non_unate:
    return true;
  case timing_sense_none:
    return false;
  default:
    return true;
  }
}

////////////////////////////////////////////////////////////////

bool
hasFanin(Vertex *vertex,
	 SearchPred *pred,
	 const Graph *graph)
{
  if (pred->searchTo(vertex)) {
    VertexInEdgeIterator edge_iter(vertex, graph);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *from_vertex = edge->from(graph);
      if (pred->searchFrom(from_vertex)
	  && pred->searchThru(edge))
	return true;
    }
  }
  return false;
}

bool
hasFanout(Vertex *vertex,
	  SearchPred *pred,
	  const Graph *graph)
{
  if (pred->searchFrom(vertex)) {
    VertexOutEdgeIterator edge_iter(vertex, graph);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      Vertex *to_vertex = edge->to(graph);
      if (pred->searchTo(to_vertex)
	  && pred->searchThru(edge))
	return true;
    }
  }
  return false;
}

} // namespace

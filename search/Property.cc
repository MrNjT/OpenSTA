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
#include "StringUtil.hh"
#include "MinMax.hh"
#include "Transition.hh"
#include "PortDirection.hh"
#include "Units.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Clock.hh"
#include "Corner.hh"
#include "PathEnd.hh"
#include "PathExpanded.hh"
#include "PathRef.hh"
#include "Property.hh"
#include "Sta.hh"
#include "Property.hh"

namespace sta {

using std::string;

static PropertyValue
pinSlewProperty(const Pin *pin,
		const TransRiseFall *tr,
		const MinMax *min_max,
		Sta *sta);
static PropertyValue
pinSlackProperty(const Pin *pin,
		 const TransRiseFall *tr,
		 const MinMax *min_max,
		 Sta *sta);
static PropertyValue
portSlewProperty(const Port *port,
		 const TransRiseFall *tr,
		 const MinMax *min_max,
		 Sta *sta);
static PropertyValue
portSlackProperty(const Port *port,
		  const TransRiseFall *tr,
		  const MinMax *min_max,
		  Sta *sta);
static PropertyValue
edgeDelayProperty(Edge *edge,
		  const TransRiseFall *tr,
		  const MinMax *min_max,
		  Sta *sta);
static float
delayPropertyValue(Delay delay,
		   Sta *sta);

////////////////////////////////////////////////////////////////

PropertyValue::PropertyValue() :
  type_(type_none)
{
  init();
}

PropertyValue::PropertyValue(const char *value) :
  type_(type_string)
{
  init();
  string_ = stringCopy(value);
}

PropertyValue::PropertyValue(std::string &value) :
  type_(type_string)
{
  init();
  string_ = stringCopy(value.c_str());
}

PropertyValue::PropertyValue(float value) :
  type_(type_float)
{
  init();
  float_ = value;
}

PropertyValue::PropertyValue(LibertyLibrary *value) :
  type_(type_liberty_library)
{
  init();
  liberty_library_ = value;
}

PropertyValue::PropertyValue(LibertyCell *value) :
  type_(type_liberty_cell)
{
  init();
  liberty_cell_ = value;
}

PropertyValue::PropertyValue(Cell *value) :
  type_(type_cell)
{
  init();
  cell_ = value;
}

PropertyValue::PropertyValue(Instance *value) :
  type_(type_instance)
{
  init();
  inst_ = value;
}

PropertyValue::PropertyValue(Pin *value) :
  type_(type_pin)
{
  init();
  pin_ = value;
}

PropertyValue::PropertyValue(PinSeq *value) :
  type_(type_pins)
{
  init();
  pins_ = value;
}

PropertyValue::PropertyValue(PinSet *value) :
  type_(type_pins)
{
  init();
  pins_ = new PinSeq;
  PinSet::Iterator pin_iter(value);
  while (pin_iter.hasNext()) {
    Pin *pin = pin_iter.next();
    pins_->push_back( pin);
  }
}

PropertyValue::PropertyValue(Net *value) :
  type_(type_net)
{
  init();
  net_ = value;
}

PropertyValue::PropertyValue(Clock *value) :
  type_(type_clock)
{
  init();
  clk_ = value;
}

PropertyValue::PropertyValue(ClockSeq *value) :
  type_(type_clocks)
{
  init();
  clks_ = new ClockSeq(*value);
}

PropertyValue::PropertyValue(ClockSet *value) :
  type_(type_clocks)
{
  init();
  clks_ = new ClockSeq;
  ClockSet::Iterator clk_iter(value);
  while (clk_iter.hasNext()) {
    Clock *clk = clk_iter.next();
    clks_->push_back(clk);
  }
}

PropertyValue::PropertyValue(PathRefSeq *value) :
  type_(type_path_refs)
{
  init();
  path_refs_ = new PathRefSeq(*value);
}

PropertyValue::PropertyValue(const PropertyValue &value) :
  type_(value.type_),
  string_(stringCopy(value.string_)),
  float_(value.float_),
  liberty_library_(value.liberty_library_),
  liberty_cell_(value.liberty_cell_),
  cell_(value.cell_),
  inst_(value.inst_),
  pin_(value.pin_),
  pins_(value.pins_ ? new PinSeq(*value.pins_) : NULL),
  net_(value.net_),
  clk_(value.clk_),
  clks_(value.clks_ ? new ClockSeq(*value.clks_) : NULL),
  path_refs_(value.path_refs_ ? new PathRefSeq(*value.path_refs_) : NULL)
{
}

void
PropertyValue::init()
{
  string_ = NULL;
  float_ = 0.0;
  liberty_library_ = NULL;
  liberty_cell_ = NULL;
  cell_ = NULL;
  inst_ = NULL;
  pin_ = NULL;
  pins_ = NULL;
  net_ = NULL;
  clk_ = NULL;
  clks_ = NULL;
  path_refs_ = NULL;
}

PropertyValue::~PropertyValue()
{  
  stringDelete(string_);
  delete clks_;
  delete pins_;
  delete path_refs_;
}

void
PropertyValue::operator=(const PropertyValue &value)
{
  type_ = value.type_;
  string_ = stringCopy(value.string_);
  float_ = value.float_;
  liberty_library_ = value.liberty_library_;
  liberty_cell_ = value.liberty_cell_;
  cell_ = value.cell_;
  inst_ = value.inst_;
  pin_ = value.pin_;
  pins_ = value.pins_ ? new PinSeq(*value.pins_) : NULL;
  net_ = value.net_;
  clk_ = value.clk_;
  clks_ = value.clks_ ? new ClockSeq(*value.clks_) : NULL;
  path_refs_ = value.path_refs_ ? new PathRefSeq(*value.path_refs_) : NULL;
}

////////////////////////////////////////////////////////////////

PropertyValue
getProperty(const Library *lib,
	    const char *property,
	    Sta *sta)
{
  auto network = sta->cmdNetwork();
  if (stringEqual(property, "name")
      || stringEqual(property, "full_name"))
    return PropertyValue(network->name(lib));
#if 0
  else if (stringEqual(property, "filename"))
    return PropertyValue(network->filename(lib));
#endif
  else
    return PropertyValue();
}

PropertyValue
getProperty(const LibertyLibrary *lib,
	    const char *property,
	    Sta *)
{
  if (stringEqual(property, "name")
      || stringEqual(property, "full_name"))
    return PropertyValue(lib->name());
  else if (stringEqual(property, "filename"))
    return PropertyValue(lib->filename());
  else
    return PropertyValue();
}

PropertyValue
getProperty(const LibertyCell *cell,
	    const char *property,
	    Sta *sta)
{
  if (stringEqual(property, "name")
      || stringEqual(property, "base_name"))
    return PropertyValue(cell->name());
  else if (stringEqual(property, "full_name")) {
    auto network = sta->cmdNetwork();
    auto lib = cell->libertyLibrary();
    const char *lib_name = lib->name();
    const char *cell_name = cell->name();
    string full_name;
    stringPrint(full_name, "%s%c%s",
		lib_name,
		network->pathDivider(),
		cell_name);
    return PropertyValue(full_name);
  }
  else if (stringEqual(property, "filename"))
    return PropertyValue(cell->filename());
  else if (stringEqual(property, "library"))
    return PropertyValue(cell->libertyLibrary());
  else
    return PropertyValue();
}

PropertyValue
getProperty(const Cell *cell,
	    const char *property,
	    Sta *sta)
{
  auto network = sta->cmdNetwork();
  if (stringEqual(property, "name")
      || stringEqual(property, "base_name"))
    return PropertyValue(network->name(cell));
  else if (stringEqual(property, "full_name")) {
    auto lib = network->library(cell);
    const char *lib_name = network->name(lib);
    const char *cell_name = network->name(cell);
    string full_name;
    stringPrint(full_name, "%s%c%s",
		lib_name,
		network->pathDivider(),
		cell_name);
    return PropertyValue(full_name);
  }
  else if (stringEqual(property, "filename"))
    return PropertyValue(network->filename(cell));
  else
    return PropertyValue();
}

////////////////////////////////////////////////////////////////

PropertyValue
getProperty(const Port *port,
	    const char *property,
	    Sta *sta)
{
  auto network = sta->cmdNetwork();
  if (stringEqual(property, "name")
	   || stringEqual(property, "full_name"))
    return PropertyValue(network->name(port));
  else if (stringEqual(property, "direction"))
    return PropertyValue(network->direction(port)->name());

  else if (stringEqual(property, "actual_fall_transition_min"))
    return portSlewProperty(port, TransRiseFall::fall(), MinMax::min(), sta);
  else if (stringEqual(property, "actual_fall_transition_max"))
    return portSlewProperty(port, TransRiseFall::fall(), MinMax::max(), sta);
  else if (stringEqual(property, "actual_rise_transition_min"))
    return portSlewProperty(port, TransRiseFall::rise(), MinMax::min(), sta);
  else if (stringEqual(property, "actual_rise_transition_max"))
    return portSlewProperty(port, TransRiseFall::rise(), MinMax::max(), sta);

  else if (stringEqual(property, "min_fall_slack"))
    return portSlackProperty(port, TransRiseFall::fall(), MinMax::min(), sta);
  else if (stringEqual(property, "max_fall_slack"))
    return portSlackProperty(port, TransRiseFall::fall(), MinMax::max(), sta);
  else if (stringEqual(property, "min_rise_slack"))
    return portSlackProperty(port, TransRiseFall::rise(), MinMax::min(), sta);
  else if (stringEqual(property, "max_rise_slack"))
    return portSlackProperty(port, TransRiseFall::rise(), MinMax::max(), sta);

  else
    return PropertyValue();
}

static PropertyValue
portSlewProperty(const Port *port,
		 const TransRiseFall *tr,
		 const MinMax *min_max,
		 Sta *sta)
{
  auto network = sta->cmdNetwork();
  Instance *top_inst = network->topInstance();
  Pin *pin = network->findPin(top_inst, port);
  return pinSlewProperty(pin, tr, min_max, sta);
}

static PropertyValue
portSlackProperty(const Port *port,
		  const TransRiseFall *tr,
		  const MinMax *min_max,
		  Sta *sta)
{
  auto network = sta->cmdNetwork();
  Instance *top_inst = network->topInstance();
  Pin *pin = network->findPin(top_inst, port);
  return pinSlackProperty(pin, tr, min_max, sta);
}

PropertyValue
getProperty(const LibertyPort *port,
	    const char *property,
	    Sta *)
{
  if (stringEqual(property, "name"))
    return PropertyValue(port->name());
  else if (stringEqual(property, "full_name"))
    return PropertyValue(port->name());
  else if (stringEqual(property, "direction"))
    return PropertyValue(port->direction()->name());
  else
    return PropertyValue();
}

////////////////////////////////////////////////////////////////

class PropertyError : public StaException
{
public:
  PropertyError(const char *type,
		const char *property);
  virtual const char *what() const throw();

protected:
  const char *type_;
  const char *property_;
};

PropertyError::PropertyError(const char *type,
			     const char *property) :
  type_(type),
  property_(property)
{
}

const char *
PropertyError::what() const throw()
{
  // leak
  return stringPrint("%s objects do not have a %s property.",
		     type_, property_);
}

////////////////////////////////////////////////////////////////

PropertyValue
getProperty(const Instance *inst,
	    const char *property,
	    Sta *sta)
{
  auto network = sta->cmdNetwork();
  if (stringEqual(property, "name"))
    return PropertyValue(network->name(inst));
  else if (stringEqual(property, "full_name"))
    return PropertyValue(network->pathName(inst));
  else if (stringEqual(property, "ref_name"))
    return PropertyValue(network->name(network->cell(inst)));
  else if (stringEqual(property, "liberty_cell"))
    return PropertyValue(network->libertyCell(inst));
  else if (stringEqual(property, "cell"))
    return PropertyValue(network->cell(inst));
  else
    throw PropertyError("instance", property);
}

////////////////////////////////////////////////////////////////

PropertyValue
getProperty(const Pin *pin,
	    const char *property,
	    Sta *sta)
{
  auto network = sta->cmdNetwork();
  if (stringEqual(property, "direction"))
    return PropertyValue(network->direction(pin)->name());
  else if (stringEqual(property, "full_name"))
    return PropertyValue(network->pathName(pin));
  else if (stringEqual(property, "lib_pin_name"))
    return PropertyValue(network->portName(pin));
  else if (stringEqual(property, "clocks")) {
    ClockSet clks;
    sta->clocks(pin, clks);
    return PropertyValue(&clks);
  }

  else if (stringEqual(property, "max_fall_slack"))
    return pinSlackProperty(pin, TransRiseFall::fall(), MinMax::max(), sta);
  else if (stringEqual(property, "max_rise_slack"))
    return pinSlackProperty(pin, TransRiseFall::rise(), MinMax::max(), sta);
  else if (stringEqual(property, "min_fall_slack"))
    return pinSlackProperty(pin, TransRiseFall::fall(), MinMax::min(), sta);
  else if (stringEqual(property, "min_rise_slack"))
    return pinSlackProperty(pin, TransRiseFall::rise(), MinMax::min(), sta);

  else if (stringEqual(property, "actual_fall_transition_max"))
    return pinSlewProperty(pin, TransRiseFall::fall(), MinMax::max(), sta);
  else if (stringEqual(property, "actual_rise_transition_max"))
    return pinSlewProperty(pin, TransRiseFall::rise(), MinMax::max(), sta);
  else if (stringEqual(property, "actual_rise_transition_min"))
    return pinSlewProperty(pin, TransRiseFall::rise(), MinMax::min(), sta);
  else if (stringEqual(property, "actual_fall_transition_min"))
    return pinSlewProperty(pin, TransRiseFall::fall(), MinMax::min(), sta);

  else
    throw PropertyError("pin", property);
}

static PropertyValue
pinSlackProperty(const Pin *pin,
		 const TransRiseFall *tr,
		 const MinMax *min_max,
		 Sta *sta)
{
  Slack slack = sta->pinSlack(pin, tr, min_max);
  return PropertyValue(delayPropertyValue(slack, sta));
}

static PropertyValue
pinSlewProperty(const Pin *pin,
		const TransRiseFall *tr,
		const MinMax *min_max,
		Sta *sta)
{
  auto graph = sta->graph();
  Vertex *vertex, *bidirect_drvr_vertex;
  graph->pinVertices(pin, vertex, bidirect_drvr_vertex);
  Slew slew = min_max->initValue();
  if (vertex) {
    Slew vertex_slew = sta->vertexSlew(vertex, tr, min_max);
    if (delayFuzzyGreater(vertex_slew, slew, min_max))
      slew = vertex_slew;
  }
  if (bidirect_drvr_vertex) {
    Slew vertex_slew = sta->vertexSlew(bidirect_drvr_vertex, tr, min_max);
    if (delayFuzzyGreater(vertex_slew, slew, min_max))
      slew = vertex_slew;
  }
  return PropertyValue(delayPropertyValue(slew, sta));
}

////////////////////////////////////////////////////////////////

PropertyValue
getProperty(const Net *net,
	    const char *property,
	    Sta *sta)
{
  auto network = sta->cmdNetwork();
  if (stringEqual(property, "full_name"))
    return PropertyValue(network->pathName(net));
  else
    throw PropertyError("net", property);
}

////////////////////////////////////////////////////////////////

PropertyValue
getProperty(Edge *edge,
	    const char *property,
	    Sta *sta)
{
  if (stringEqual(property, "full_name")) {
    auto network = sta->cmdNetwork();
    auto graph = sta->graph();
    const char *from = edge->from(graph)->name(network);
    const char *to = edge->to(graph)->name(network);
    return stringPrintTmp("%s -> %s", from, to);
  }
  if (stringEqual(property, "delay_min_fall"))
    return edgeDelayProperty(edge, TransRiseFall::fall(), MinMax::min(), sta);
  else if (stringEqual(property, "delay_max_fall"))
    return edgeDelayProperty(edge, TransRiseFall::fall(), MinMax::max(), sta);
  else if (stringEqual(property, "delay_min_rise"))
    return edgeDelayProperty(edge, TransRiseFall::rise(), MinMax::min(), sta);
  else if (stringEqual(property, "delay_max_rise"))
    return edgeDelayProperty(edge, TransRiseFall::rise(), MinMax::max(), sta);
  else if (stringEqual(property, "sense"))
    return PropertyValue(timingSenseString(edge->sense()));
  else if (stringEqual(property, "from_pin"))
    return PropertyValue(edge->from(sta->graph())->pin());
  else if (stringEqual(property, "to_pin"))
    return PropertyValue(edge->to(sta->graph())->pin());
  else
    throw PropertyError("edge", property);
}

static PropertyValue
edgeDelayProperty(Edge *edge,
		  const TransRiseFall *tr,
		  const MinMax *min_max,
		  Sta *sta)
{
  ArcDelay delay = 0.0;
  bool delay_exists = false;
  TimingArcSet *arc_set = edge->timingArcSet();
  TimingArcSetArcIterator arc_iter(arc_set);
  while (arc_iter.hasNext()) {
    TimingArc *arc = arc_iter.next();
    TransRiseFall *to_tr = arc->toTrans()->asRiseFall();
    if (to_tr == tr) {
      CornerIterator corner_iter(sta);
      while (corner_iter.hasNext()) {
	Corner *corner = corner_iter.next();
	DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(min_max);
	ArcDelay arc_delay = sta->arcDelay(edge, arc, dcalc_ap);
	if (!delay_exists
	    || ((min_max == MinMax::max()
		 && arc_delay > delay)
		|| (min_max == MinMax::min()
		    && arc_delay < delay)))
	  delay = arc_delay;
      }
    }
  }
  return PropertyValue(delayPropertyValue(delay, sta));
}

////////////////////////////////////////////////////////////////

PropertyValue
getProperty(TimingArcSet *arc_set,
	    const char *property,
	    Sta *)
{
  if (stringEqual(property, "name")
      || stringEqual(property, "full_name")) {
    auto from = arc_set->from()->name();
    auto to = arc_set->to()->name();
    auto cell_name = arc_set->libertyCell()->name();
    string name;
    stringPrint(name, "%s %s -> %s", cell_name, from, to);
    return PropertyValue(name);
  }
  else
    throw PropertyError("timing arc", property);
}

////////////////////////////////////////////////////////////////

PropertyValue
getProperty(Clock *clk,
	    const char *property,
	    Sta *sta)
{
  if (stringEqual(property, "name")
      || stringEqual(property, "full_name"))
    return PropertyValue(clk->name());
  else if (stringEqual(property, "period"))
    return PropertyValue(sta->units()->timeUnit()->asString(clk->period(), 8));
  else if (stringEqual(property, "sources"))
    return PropertyValue(clk->pins());
  else if (stringEqual(property, "propagated"))
    return PropertyValue(clk->isPropagated() ? "1" : "0");
  else
    throw PropertyError("clock", property);
}

////////////////////////////////////////////////////////////////

PropertyValue
getProperty(PathEnd *end,
	    const char *property,
	    Sta *sta)
{
  if (stringEqual(property, "startpoint")) {
    PathExpanded expanded(end->path(), sta);
    return PropertyValue(expanded.startPath()->pin(sta));
  }
  else if (stringEqual(property, "startpoint_clock"))
    return PropertyValue(end->path()->clock(sta));
  else if (stringEqual(property, "endpoint"))
    return PropertyValue(end->path()->pin(sta));
  else if (stringEqual(property, "endpoint_clock"))
    return PropertyValue(end->targetClk(sta));
  else if (stringEqual(property, "endpoint_clock_pin"))
    return PropertyValue(end->targetClkPath()->pin(sta));
  else if (stringEqual(property, "slack"))
    return PropertyValue(delayPropertyValue(end->slack(sta), sta));
  else if (stringEqual(property, "points")) {
    PathExpanded expanded(end->path(), sta);
    PathRefSeq paths;
    for (auto i = expanded.startIndex(); i < expanded.size(); i++) {
      PathRef *path = expanded.path(i);
      paths.push_back(*path);
    }
    return PropertyValue(&paths);
  }
  else
    throw PropertyError("path end", property);
}

PropertyValue
getProperty(PathRef *path,
	    const char *property,
	    Sta *sta)
{
  if (stringEqual(property, "pin"))
    return PropertyValue(path->pin(sta));
  else if (stringEqual(property, "arrival"))
    return PropertyValue(delayPropertyValue(path->arrival(sta), sta));
  else if (stringEqual(property, "required"))
    return PropertyValue(delayPropertyValue(path->required(sta), sta));
  else if (stringEqual(property, "slack"))
    return PropertyValue(delayPropertyValue(path->slack(sta), sta));
  else
    throw PropertyError("path", property);
}

static float
delayPropertyValue(Delay delay,
		   Sta *sta)
{
  return delayAsFloat(delay) / sta->units()->timeUnit()->scale();
}

} // namespace

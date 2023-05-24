#include "actions.h"

using namespace std;

namespace itcoin {
namespace pbft {
namespace actions {

Action::Action(uint32_t replica_id): m_replica_id(replica_id)
{

}

Action::Action(PlTerm Replica_id): m_replica_id((long) Replica_id)
{

}

std::string Action::name() const
{
  return ACTION_TYPE_TYPE_AS_STRING[static_cast<size_t>(this->type())];
}

std::optional<std::reference_wrapper<const messages::Message>> Action::message() const
{
  return std::nullopt;
}

std::ostream& operator<<(std::ostream& Str, const Action& action)
{
  // print something from v to str, e.g: Str << v.getX();
  string action_str = action.identify();
  Str << action_str;
  return Str;
}

}
}
}

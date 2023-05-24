# I/O automata pseudocode

This pseudocode describes the processing of messages and other events in the I/O Automata model of Castro Liskov 2022.

```python

while (True):

  # If there are not other actions to activate, we await for events
  if not state.active_actions.next() and not new_events.next():
    await new_events.wait()

  # A new event can be either:
  # 1. a new message from network, or
  while (new_events.next()):
    event = new_events.pop()
    state.AddEvent(event)

  # We execute a random active actions
  action_to_activate = state.GetRandomActiveAction()
  state.ApplyAction(action_to_activate)

  # We send messages to other replicas according to the applied effects
  while(state.out_messages.next())
    msg = state.out_messages.pop()
    network.send(msg)

```

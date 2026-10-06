# Input and Gesture Service

Normalizes CST9217 touch input into platform events.

Events include:
- tap
- long press
- swipe up
- swipe down
- swipe left
- swipe right

The top-edge swipe-down gesture is reserved for the system Quick Controls.

Suggested pipeline:

Touch driver -> Input Service -> Gesture recognition -> System/Game dispatcher

System gestures are dispatched before active-game input.

target_sources_ifdef(
  CONFIG_ZMK_CUSTOM_BEHAVIORS
  zmk
  PRIVATE
  behavior_layout_switch.c
)
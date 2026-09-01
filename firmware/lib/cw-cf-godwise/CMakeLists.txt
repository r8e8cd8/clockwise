cmake_minimum_required(VERSION 3.5)

idf_component_register(
  INCLUDE_DIRS "."
  SRC_DIRS "."
  REQUIRES arduino Adafruit-GFX-Library cw-gfx-engine cw-commons
)
project(cw-cf-godwise)

Advanced Game Development

Final Project Proposal

Ethan Aitken, Nathaniel Major

-------------------------------------------------

Zero-Gravity Pool
Billiards, in space!

-------------------------------------------------


Summary

Traditional Billiards, with a twist. Instead of playing on a table, the playing field is a giant
cube where all balls are suspended in a zero-gravity environment with wind resistance. The
goal of the game is for you (the white ball) to sink all of one type of ball (stripe or solids)
before your opponent can. You line up your shot in the first-person camera view of the
white ball and can then shoot with varied power levels and can then observe the result of
the shot in a free roam camera view. Physics collisions will move balls around the playing
field, where the “holes” you sink the balls in will be positioned in each corner of the cube,
as well as the midpoint of each edge.

-------------------------------------------------


Features

Mathematical Foundations:
1. Applied forces, wind resistance, resultant vector formulas for trajectory,
accelerations, etc. All come into play for ball transformations around the playing
field

Shape Representations:
1. Sphere (balls)
2. Cube (playing field)
3. Other non-ordinary shapes. Obstacles, debris, etc to float around the playing field.

Camera:
1. First-person view controllable by mouse movement of the white ball
2. Free roam camera to observe the result of a shot

Illumination:
1. Standard lighting of the entire world. No complicated shadows are planned but will
be implemented if time allows.

Real-time rendering:
1. All moving parts will be rendered real-time as they change position

Textures:
1. Shiny / sheen textures for the balls
2. Rough / rugged / turf textures for the walls of cube
3. Wood textures for edges and holes
4. Misc. textures for any debris added to game

Visual Effects:
1. Line of expectant trajectory to protrude from the ball expected to be hit first by the
white ball on a player’s turn

Particle System:
1. Particle trail following the trajectory of a moving ball
2. Small effect upon two balls colliding

Splines:
1. May be used to create a pan-shot of the free roam camera, to provide a clear
unobstructed view of the entire playing field without player input

Collision Detection:
1. Dynamic sphere on sphere collision
2. Sphere to static object
3. Sphere to “hole” collision (when ball is sunk in a pocket)

Physically based shading model:
1. Unsure of the implications of this – will tackle once covered in class + if time allows

Raytracing:
1. Not planned – will tackle if time allows
1. Unsure of the implications of this – will tackle once covered in class + if time allows
Raytracing:
1. Not planned – will tackle if time allows

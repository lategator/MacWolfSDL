project('macwolfsdl', 'c', default_options: ['default_library=static'])
cc = meson.get_compiler('c')
srcs = [
  'Burger.c', 'Data.c', 'Doors.c', 'EnMove.c', 'EnThink.c', 'InterMis.c',
  'Intro.c', 'Level.c', 'MacFont.c', 'MacLoader.c', 'MacMenu.c', 'Missiles.c',
  'Music.c', 'PlMove.c', 'PlStuff.c', 'PlThink.c', 'PushWall.c', 'RefBsp.c',
  'Refresh2.c', 'Refresh.c', 'RefSprite.c', 'SDLWolf.c', 'SetupScalers.c',
  'Sight.c', 'SnesMain.c', 'StateDef.c', 'WolfIO.c', 'WolfMain.c', 'ini.c',
  'res.c',
]
deps = [
  dependency('sdl3', version: '>= 3.2'),
  cc.find_library('m', required : false)
]
executable(meson.project_name(), srcs, dependencies: deps)


Patches for Crossfire go in this folder: any .ini file here is read after Crossfire_Rules.ini, in alphabetical
order, with the same sections ([Elements], [Reactions], [Exclude]). A patch for a spell mod might add its own
element keywords and exclude a utility projectile:

  [Elements]
  Frost=+MySpellMod_FrostKeyword

  [Exclude]
  Forms=MySpellMod.esp|0x000D62

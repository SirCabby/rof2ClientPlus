# RoF2 client model inventory — classic vs. replaced

*Generated 2026-07-13 by `tools/model_inventory.py` from `/home/joshua/Games/RoF2`.*
*Regenerate: `python3 tools/model_inventory.py`. A model is **replaced** when its code is defined in more than one loaded archive — the higher-priority one wins (`.eqg` beats `.s3d`; later `GlobalLoad.txt` entry beats earlier), hiding the classic look.*

## Summary

|  | count |
|---|---|
| gequip*.s3d weapon archives | 7 |
| *equip*.eqg override archives | 14 |
| distinct IT weapon models | 1669 |
| &nbsp;&nbsp;named from DB | 1458 |
| **weapon models REPLACED** | **23** |
| global*_chr.s3d archives | 37 |
| distinct global char/NPC codes | 63 |
| **global models REPLACED** | **30** |
| character/creature .eqg archives | 381 |
| zone _chr.s3d archives scanned | 356 |
| distinct NPC codes (global+zone) | 448 |
| &nbsp;&nbsp;named (best-effort map) | 86 / 448 |

## Weapons — replaced models

Classic `IT##` weapon meshes that a higher-priority archive overrides. Item names come from the EQEmu DB (`items.idfile`) — one model backs many items, so the 2–3 shortest examples are shown.

| model | in-game name (examples) | classic source (.s3d) | overridden by |
|---|---|---|---|
| IT7 | Mace / Jade Mace / Rusty Mace | gequip.s3d | equipment-01.eqg |
| IT14 | Gavel / Warhammer / Fist of Zek | gequip.s3d | equipment-01.eqg |
| IT15 | Horn / Ivory Horn / Brahhms Horn | gequip.s3d | equipment-01.eqg |
| IT18 | Club / Patonk / Blackjack | gequip.s3d | equipment-01.eqg |
| IT23 | Halberd / Iceflame / Sunderfury | gequip.s3d | equipment-01.eqg |
| IT24 | Man-o-War / Titans Fist / Centi Warhammer | gequip.s3d | equipment-01.eqg |
| IT30 | Blood Fork / Goblin Poker / Spine Piercer | gequip.s3d | equipment-01.eqg |
| IT31 | Warclub / Wintry Club / Neb's Warbone | gequip.s3d | equipment-01.eqg |
| IT32 | Broom / Everriculum / Giant's Broom | gequip.s3d | equipment-01.eqg |
| IT39 | Beckon / Scythe / Harvester | gequip.s3d | equipment-01.eqg |
| IT40 | Bloodmoon / Sickle of Urash / Bronze Kusarigama | gequip.s3d | equipment-01.eqg |
| IT41 | Machete / Scimitar / Surgical Saw | gequip.s3d | equipment-01.eqg |
| IT42 | Blood Fire / Soul Defiler / Soul Devourer | gequip.s3d | equipment-01.eqg |
| IT43 | Pick / Gem Finder / Forged Pick | gequip.s3d | equipment-01.eqg |
| IT52 | Shovel / Gravebinder / Wee Harvester | gequip.s3d | equipment-01.eqg |
| IT58 | Bastard Sword / Vacra Av Svim / Sword of Slate | gequip.s3d | equipment-01.eqg |
| IT60 | Shlis / Onyxbrand / Battle Axe | gequip.s3d | equipment-01.eqg |
| IT61 | Bola / Vervain / Bull Whip | gequip.s3d | equipment-01.eqg |
| IT67 | The Idol / Hag Totem / Icy Skull | gequip.s3d | equipment-01.eqg |
| IT79 | Coral Trident / Girplan's Tail / Charger's Lance | gequip.s3d | equipment-01.eqg |
| IT81 | Maelstrom / Zephyrwind / Lamentation | gequip.s3d | equipment-01.eqg |
| IT10524 | Icefloe Hammer / Hammer of Hours / Jagged Coral Sledge | gequip5.s3d, gequip8.s3d | (later .s3d) |
| IT11017 | Crow Wing / Nymph Wing / Gnomium Sample | gequip3.s3d, gequip4.s3d | (later .s3d) |

## Weapons — full IT index

| model | in-game name (examples) | .s3d archives | .eqg archives | status |
|---|---|---|---|---|
| IT1 | Grip of Fear | gequip.s3d | — | classic only |
| IT2 | Diffusing Drape / Damaged Fine Steel Greatsword / Ancient Mithril Two-Handed Sword | gequip.s3d | — | classic only |
| IT3 |  | gequip.s3d | — | classic only |
| IT4 | Diminutive Bow / Ancient Runed Oak Bow | gequip.s3d | — | classic only |
| IT5 | Dagger* / Boneshear / Frozen Key | gequip.s3d | — | classic only |
| IT6 | Archaic Flute / Blood Red Flute / Kurlok's Maddening Song | gequip.s3d | — | classic only |
| IT7 | Mace / Jade Mace / Rusty Mace | gequip.s3d | equipment-01.eqg | REPLACED (.eqg over .s3d) |
| IT8 | Staff / Bo Stick / Flame Bolt | gequip.s3d | — | classic only |
| IT9 | Sturdy Defensive Axe | gequip.s3d | — | classic only |
| IT10 | Arrow / Stingers / Kyv Arrow | gequip.s3d | — | classic only |
| IT11 | Sword of the Gathering / Master Sword of the Gathering | gequip2.s3d | — | classic only |
| IT12 | Staff of the Gathering / War Staff of the Gathering / Master Staff of the Gathering | gequip2.s3d | — | classic only |
| IT13 | Scepter of Al`Kabor / Quintessence of Elements / Spectral Containment Device | gequip.s3d | — | classic only |
| IT14 | Gavel / Warhammer / Fist of Zek | gequip.s3d | equipment-01.eqg | REPLACED (.eqg over .s3d) |
| IT15 | Horn / Ivory Horn / Brahhms Horn | gequip.s3d | equipment-01.eqg | REPLACED (.eqg over .s3d) |
| IT16 | Spear of the Chosen | gequip.s3d | — | classic only |
| IT17 | Morky's Spear / Ancient Runed Bone Fork | gequip.s3d | — | classic only |
| IT18 | Club / Patonk / Blackjack | gequip.s3d | equipment-01.eqg | REPLACED (.eqg over .s3d) |
| IT19 | Ancient Sharkbone Warhammer | gequip.s3d | — | classic only |
| IT20 | Foil / Rapier / Eyerazzia | gequip.s3d | — | classic only |
| IT21 |  | gequip.s3d | — | classic only |
| IT22 |  | gequip.s3d | — | classic only |
| IT23 | Halberd / Iceflame / Sunderfury | gequip.s3d | equipment-01.eqg | REPLACED (.eqg over .s3d) |
| IT24 | Man-o-War / Titans Fist / Centi Warhammer | gequip.s3d | equipment-01.eqg | REPLACED (.eqg over .s3d) |
| IT25 | Dull Axe* / Fleshripper / Brutechopper | gequip.s3d | — | classic only |
| IT26 |  | gequip.s3d | — | classic only |
| IT27 | Journal / Torn Tome / Walrus Tusk | gequip.s3d | — | classic only |
| IT28 | Well Worn Book / Shirik's Missive / Incriminating Note | gequip.s3d | — | classic only |
| IT29 | Blood Skull / Petrified Rod / Skullsplitter | gequip.s3d | — | classic only |
| IT30 | Blood Fork / Goblin Poker / Spine Piercer | gequip.s3d | equipment-01.eqg | REPLACED (.eqg over .s3d) |
| IT31 | Warclub / Wintry Club / Neb's Warbone | gequip.s3d | equipment-01.eqg | REPLACED (.eqg over .s3d) |
| IT32 | Broom / Everriculum / Giant's Broom | gequip.s3d | equipment-01.eqg | REPLACED (.eqg over .s3d) |
| IT33 |  | gequip.s3d | — | classic only |
| IT34 |  | gequip.s3d | — | classic only |
| IT35 | Rod / Scepter / Diamond Rod | gequip.s3d | — | classic only |
| IT36 | Torch / Wax Candle / Torch of Ro | gequip.s3d | — | classic only |
| IT37 |  | gequip.s3d | — | classic only |
| IT38 | Fishing Pole / Kerran Fishingpole / Blessed Fishing Rod | gequip.s3d | — | classic only |
| IT39 | Beckon / Scythe / Harvester | gequip.s3d | equipment-01.eqg | REPLACED (.eqg over .s3d) |
| IT40 | Bloodmoon / Sickle of Urash / Bronze Kusarigama | gequip.s3d | equipment-01.eqg | REPLACED (.eqg over .s3d) |
| IT41 | Machete / Scimitar / Surgical Saw | gequip.s3d | equipment-01.eqg | REPLACED (.eqg over .s3d) |
| IT42 | Blood Fire / Soul Defiler / Soul Devourer | gequip.s3d | equipment-01.eqg | REPLACED (.eqg over .s3d) |
| IT43 | Pick / Gem Finder / Forged Pick | gequip.s3d | equipment-01.eqg | REPLACED (.eqg over .s3d) |
| IT44 | Ancient Electrum-Bladed Wakizashi | gequip.s3d | — | classic only |
| IT45 | Gnarled Staff / Nargon's Staff / Wand of Chitin | gequip.s3d | — | classic only |
| IT46 | Wand / Ebon Wand / Rod of Bone | gequip.s3d | — | classic only |
| IT47 | Silver Wand / Wand of Ice / Red Sparkler | gequip.s3d | — | classic only |
| IT48 | Boat Beacon / Spirit Cage / Crooked Cage | gequip.s3d | — | classic only |
| IT49 | Gardash / War Maul / Big Bruiser | gequip.s3d | — | classic only |
| IT50 | Rusted Blade / Ancient Serrated Bone Dirk | gequip.s3d | — | classic only |
| IT51 | Memory / Golden Rod / Mindstealer | gequip.s3d | — | classic only |
| IT52 | Shovel / Gravebinder / Wee Harvester | gequip.s3d | equipment-01.eqg | REPLACED (.eqg over .s3d) |
| IT53 | Craslith / Rheumguls / Dark Reaver | gequip.s3d | — | classic only |
| IT54 | Belt / Crinkled Note / The Last Cigar | gequip.s3d | — | classic only |
| IT55 | Old Pipe / Whip Stock / Captain Greyheart's Pipe | gequip.s3d | — | classic only |
| IT56 | Stein / Azure Stein / Crude Stein | gequip.s3d | — | classic only |
| IT57 |  | gequip.s3d | — | classic only |
| IT58 | Bastard Sword / Vacra Av Svim / Sword of Slate | gequip.s3d | equipment-01.eqg | REPLACED (.eqg over .s3d) |
| IT59 | Dwarf Mining Pick / Grandmaster Brewer's Corker | gequip.s3d | — | classic only |
| IT60 | Shlis / Onyxbrand / Battle Axe | gequip.s3d | equipment-01.eqg | REPLACED (.eqg over .s3d) |
| IT61 | Bola / Vervain / Bull Whip | gequip.s3d | equipment-01.eqg | REPLACED (.eqg over .s3d) |
| IT62 | SoulFire / Fiery Avenger / Ancient SoulFire | gequip.s3d | — | classic only |
| IT63 | Air / Ale / Box | gequip.s3d | — | classic only |
| IT64 | Cork / Fire / Gold | gequip.s3d | — | classic only |
| IT65 | Eternity / Playbill / Fetid Pus | gequip.s3d | — | classic only |
| IT66 | Forge / Small Roller / Steamjet Pack | gequip.s3d | — | classic only |
| IT67 | The Idol / Hag Totem / Icy Skull | gequip.s3d | equipment-01.eqg | REPLACED (.eqg over .s3d) |
| IT68 | Fetid Orb / Riftstone / Shrubbery | gequip.s3d | — | classic only |
| IT69 | Ordinary Lure /  New Tanaan Oven | gequip.s3d | — | classic only |
| IT70 | Bolts / Brewing Barrel / New Tanaan Brewing Barrel | gequip.s3d | — | classic only |
| IT71 | Stormclaws / Vius Claws / Feral Claws | gequip.s3d | — | classic only |
| IT72 | Epositrig / Manastone / Fatesealer | gequip.s3d | — | classic only |
| IT73 | New Tanaan Kiln / Scrap of Dark Matter | gequip.s3d | — | classic only |
| IT74 | Simple Gauze Press / New Tanaan Pottery Wheel / Tinmizer's Stupendous Contraption | gequip.s3d | — | classic only |
| IT75 | Micah / Duir Staff / Shillelagh | gequip.s3d | — | classic only |
| IT76 | Rusted Water Pump / Shamshir of the Disciple / Shar Vahl Garrison Blade | gequip.s3d | — | classic only |
| IT78 | Small Clockwork Cat | gequip.s3d | — | classic only |
| IT79 | Coral Trident / Girplan's Tail / Charger's Lance | gequip.s3d | equipment-01.eqg | REPLACED (.eqg over .s3d) |
| IT80 | Jade Reaver / Weighted Axe / Ashenbone Axe | gequip.s3d | — | classic only |
| IT81 | Maelstrom / Zephyrwind / Lamentation | gequip.s3d | equipment-01.eqg | REPLACED (.eqg over .s3d) |
| IT82 | Pawn's Khukri / Rusty Fer`Esh / Forged Fer`Esh | gequip.s3d | — | classic only |
| IT83 | Fangol / Simple Spring / The Harbinger | gequip.s3d | — | classic only |
| IT84 | Palladium Axe / Simple Pulley / Efreeti War Axe | gequip.s3d | — | classic only |
| IT85 | Reaver / Serrated Blade / Sharkjaw Cutlass | gequip.s3d | — | classic only |
| IT86 | Espri / Bonechiller / Briarzephyr | gequip.s3d | — | classic only |
| IT87 | Bent Sword / Six Note Blade / Kylong War Dagger | gequip.s3d | — | classic only |
| IT88 | Shieldstorm / Swarmcaller / Runic Carver | gequip.s3d | — | classic only |
| IT89 | Sarnak Liberator / Glaive of Marltek / Hierophant's Knife | gequip.s3d | — | classic only |
| IT90 | Soul's Eye / Tarantella / Blade of Passage | gequip.s3d | — | classic only |
| IT91 | Rusty Ulak / Steel Ulak / Bronze Ulak | gequip.s3d | — | classic only |
| IT92 | Slave / Wurmslayer / Canyoneer Pike | gequip.s3d | — | classic only |
| IT93 | Kolumejalima / Entropic Cleaver / Ornate Rune Blade | gequip.s3d | — | classic only |
| IT94 | Willsapper / Dragon's Claw / Bearfang Fists | gequip.s3d | — | classic only |
| IT95 | Felhammer / Earthshaker / Glavus' Blade | gequip.s3d | — | classic only |
| IT96 | The Lifeguide / Crowned Staff of Anguish / Staff of Primordial Ooze | gequip.s3d | — | classic only |
| IT97 | Celestial Sword | gequip.s3d | — | classic only |
| IT98 | Blightfang / Spell: Hex / Chaos Runes | gequip.s3d | — | classic only |
| IT99 | Tin Ore / Dynamite / Rat Meat | gequip.s3d | — | classic only |
| IT100 | Wavecrasher / Steel So'Shar / Firerune Brand | gequip.s3d | — | classic only |
| IT101 | Staff of Pain / Clawscale Staff / Ketchata Koro Mis | gequip.s3d | — | classic only |
| IT102 | Dwarven Sap / Bestial Club / Darkthorn Mace | gequip.s3d | — | classic only |
| IT103 | Bronze Yari / Grimy Lance / Hunter's Pike | gequip.s3d | — | classic only |
| IT104 | Hero's Khukri / Kunzar Ku'juch / Black Scale Blade | gequip.s3d | — | classic only |
| IT105 | Rusty Shan`Tok / Forged Shan`Tok / Skyiron Shan`Tok | gequip.s3d | — | classic only |
| IT106 | The Silver Nemesis / Jeweled Slate Sword / Pockmarked Short Sword | gequip.s3d | — | classic only |
| IT107 | Tu'Nakk Parryblade / Shadow-Forged Bonesaw / Shiliskin Barbed Sword | gequip.s3d | — | classic only |
| IT108 | Heartsbane / Steel Ga'Nak / Blade of Chaos | gequip.s3d | — | classic only |
| IT109 | Sword of Skyfire / Jarsath Battle Sword | gequip.s3d | — | classic only |
| IT110 | Leg-Chopper / Savage Warblade / The Sword of War | gequip.s3d | — | classic only |
| IT111 | Tnor'Tesphi / Rod of Battle / Rubicite Staff | gequip.s3d | — | classic only |
| IT112 | Club* / Blam Stick / Skullcrusher | gequip.s3d | — | classic only |
| IT113 | Steel Ta'Nak / Edge of the People / Vengeful Diamond Blade | gequip.s3d | — | classic only |
| IT115 | Dark Ember / Facesmasher / Hammer of Battle | gequip.s3d | — | classic only |
| IT117 | Steel Re'Stek / Truncheon of Doom / Hammer of Cessation | gequip.s3d | — | classic only |
| IT118 | Envy / Thistlepoint / Slave Piercer | gequip.s3d | — | classic only |
| IT119 | Fester / Bloodbath / Bonecurse | gequip.s3d | — | classic only |
| IT120 | Shaded blade / Spirit Reaver / Marauder's Blade | gequip.s3d | — | classic only |
| IT121 | Grimy Blade / Blood Onyx Blade / Breath of Harmony | gequip.s3d | — | classic only |
| IT122 | Slimed Staff / Baton of Faith / Gray Metal Mace | gequip.s3d | — | classic only |
| IT123 | Xolion Rod / Shaded staff / Steel Ch'Ror | gequip.s3d | — | classic only |
| IT124 | Soul Binder / Caen's Bo Staff of Fury | gequip.s3d | — | classic only |
| IT125 | Staff of Null / Tserrina's Staff / Rod of Ulceration | gequip.s3d | — | classic only |
| IT126 | Glowing Club / Rotwood Club / Muddied Staff | gequip.s3d | — | classic only |
| IT127 | Iceshaper's Staff / Dragon Spine Staff / Di`zok Escape Staff | gequip.s3d | — | classic only |
| IT128 | New Tanaan Loom | gequip.s3d | — | classic only |
| IT129 | Ceremonial Sh'Voth / Deathbringer's Rod / Skull-topped Runestaff | gequip.s3d | — | classic only |
| IT130 | Mace / Bone Mace / Northman Mace | gequip.s3d | — | classic only |
| IT131 | Defiance / Kunzar Tu'Lal / Poxan's Sword | gequip.s3d | — | classic only |
| IT132 | Apparition / Goranga Spear / Sargasso Spear | gequip.s3d | — | classic only |
| IT133 | Dark Web Bowl / Diviner's Bowl / Seasalt Censer | gequip.s3d | — | classic only |
| IT134 | Root Tender Mace / Sanative Truncheon / Statuette of Trushar | gequip.s3d | — | classic only |
| IT135 | Mizzenblast FUSOR | gequip.s3d | — | classic only |
| IT136 | Vius Claws / Lunar Claws / Wyvern Claw | gequip.s3d | — | classic only |
| IT137 | Drums of the Beast / Nostrolo Tambourine / Tambourine of Rituals | gequip.s3d | — | classic only |
| IT138 | Mandolin / Kangon Lyre / Rod of Alsa Thel | gequip.s3d | — | classic only |
| IT139 | Teir`Dal Sai / Jaziriiz's Bloodblade / Dagger of the White Rose | gequip.s3d | — | classic only |
| IT140 | Ragebringer / Cracked Ragebringer / Morden's Ragebringer | gequip.s3d | — | classic only |
| IT141 | Blade of Tactics / Brittle Blade of Tactics / Glowing Blade of Tactics | gequip.s3d | — | classic only |
| IT142 | Blade of Strategy / Brittle Blade of Strategy / Glowing Blade of Strategy | gequip.s3d | — | classic only |
| IT145 | Soulstealer / Innoruuk's Curse / Blade of Cazic Thule | gequip.s3d | — | classic only |
| IT146 | Jagged Blade of War / Jagged Blade of War Ornamentation | gequip.s3d | — | classic only |
| IT148 | Singing Short Sword / Singing Short Sword Ornamentation | gequip.s3d | — | classic only |
| IT149 | Swiftwind / Earthcaller / Earthcaller Ornamentation | gequip.s3d | — | classic only |
| IT150 | Ivyweaver's Scimitar / Nature Walkers Scimitar / Nature Walkers Scimitar Ornamentation | gequip.s3d | — | classic only |
| IT151 | Orb of Mastery / Ornate Orb of Mastery / Orb of Mastery Ornamentation | gequip.s3d | — | classic only |
| IT153 | Scythe of the Shadowed Soul / Scythe of the Shadowed Soul Ornamentation | gequip.s3d | — | classic only |
| IT154 | Spear of Fate / Spear of Fate Ornamentation | gequip.s3d | — | classic only |
| IT155 | Mathii's Staff / Staff of the Four / Staff of the Four Ornamentation | gequip.s3d | — | classic only |
| IT156 | Yalienn's Water Sprinkler / Water Sprinkler of Nem Ankh / Mennelle's Worn Morning Star | gequip.s3d | — | classic only |
| IT157 | Runed Serpent Staff / Staff of the Serpent / Staff of the Serpent Ornamentation | gequip.s3d | — | classic only |
| IT159 | Celestial Fists / Tainted Celestial Fists / Celestial Fists Ornamentation | gequip.s3d | — | classic only |
| IT160 | Fiery Defender / Doused Fiery Defender / Fiery Defender Ornamentation | gequip.s3d | — | classic only |
| IT161 | Guardian Sword / Drakkel Icereaver / Silver Blade of Rot | gequip2.s3d | — | classic only |
| IT162 | Jeldorin / Frostwrath / Blade of the First Egg | gequip2.s3d | — | classic only |
| IT163 | Smolder / Chillsteeper / Djinni War Blade | gequip2.s3d | — | classic only |
| IT164 | Essence Blade / Kreizenn's Flame / Blade of the Swarm | gequip2.s3d | — | classic only |
| IT165 | Fiercewind / Warders Blade / Scimitar of Oak | gequip2.s3d | — | classic only |
| IT166 | Natures Defender / Holy Sword Ornament / The Sword of Ssraeshza | gequip2.s3d | — | classic only |
| IT167 | Heavy Dagger / Shadow Rager / Golden Dagger | gequip2.s3d | — | classic only |
| IT168 | Sainy's Claw / Velium Warsword / Shissar Warsword | gequip2.s3d | — | classic only |
| IT169 | Vius maul / Lunar maul / Astral maul | gequip2.s3d | — | classic only |
| IT170 | Revtu's Axe / Steel Great Axe / Mithril Great Axe | gequip2.s3d | — | classic only |
| IT171 | Fatecaller / Frostbringer / Velium Battle Axe | gequip2.s3d | — | classic only |
| IT172 | Velium Spear / Saldaren Spear / Gray Metal Pike | gequip2.s3d | — | classic only |
| IT173 | Brownie Slasher / Centi Longsword / Volnin, the Butcher | gequip2.s3d | — | classic only |
| IT174 | Rekeklo's War Sword / Ans Xundrau xi Xauninae / Kel Xundrau xi Xauninae | gequip2.s3d | — | classic only |
| IT175 | Flamesong / Greenmist / Bronze Katana | gequip2.s3d | — | classic only |
| IT176 | Culler / Bronze Naginata / Lance of Thunder | gequip2.s3d | — | classic only |
| IT177 | Skullcracker / Ans Fiel`Tian / Crystal Hammer | gequip2.s3d | — | classic only |
| IT178 | Rocksmasher / Sentry Maul / Scalecracker | gequip2.s3d | — | classic only |
| IT179 | Glacierbone Hammer / Unsuspected Striker / Shissar Battlehammer | gequip2.s3d | — | classic only |
| IT180 | Servant's Blade / Crystallized Blade / Crystallized Sword | gequip2.s3d | — | classic only |
| IT181 | Frostreaver / Iron Great Axe / Bronze Great Axe | gequip2.s3d | — | classic only |
| IT182 | Frozen Great Axe / Glowing Velium Axe / Doljonijiarnimorinar | gequip2.s3d | — | classic only |
| IT183 | Vinecutter / Elven Sun Blade / Stolen Sun Blade | gequip2.s3d | — | classic only |
| IT184 | Infestation / Ivyshae Blade / Nightgrowth Blade | gequip2.s3d | — | classic only |
| IT185 | Queenlash Hammer / Mace of Focused Flame / Cracked Granite Hammer | gequip2.s3d | — | classic only |
| IT186 | Velium Great Staff / Amygdalan War Staff / Staff of the Chamberlain | gequip2.s3d | — | classic only |
| IT187 | Velium Spear / Shardwurm Stinger / Spear of Permafrost | gequip2.s3d | — | classic only |
| IT188 | Velium Long Sword / Glassy Short Sword / Giant Militia Longsword | gequip2.s3d | — | classic only |
| IT189 | Velium Morning Star / Purified Crystal Star / Crystallized Shadow Star | gequip2.s3d | — | classic only |
| IT190 | Silver Plated War Sword / Velium Two Handed Sword / Purified Crystal Claymore | gequip2.s3d | — | classic only |
| IT191 | Frozen Blade / Velium Short Sword / Short Sword of Permafrost | gequip2.s3d | — | classic only |
| IT192 | Velium Scimitar / Crystal Scimitar / Purified Scimitar | gequip2.s3d | — | classic only |
| IT193 | Velium Warhammer / Coldain Velium Warhammer / Purified Crystal Warhammer | gequip2.s3d | — | classic only |
| IT194 | Chill Dagger / Frosted Claw / Siver's Claw | gequip2.s3d | — | classic only |
| IT195 | Velium Rapier / Frosted Ice Spike / Coldain Velium Rapier | gequip2.s3d | — | classic only |
| IT196 | Snowchipper / Regla's Pick / Burynai Scoutpick | gequip2.s3d | — | classic only |
| IT197 | Vius Lance / Lunar Lance / Astral Lance | gequip2.s3d | — | classic only |
| IT198 | Breezeboot's Frigid Gnasher | gequip2.s3d | — | classic only |
| IT199 | Terror Crossbow / Coldain Crossbow / Nadia's Crossbow | gequip.s3d | — | classic only |
| IT200 | Fungoid Sap / Meteor Dust / The Qeynos Shield | gequip.s3d | — | classic only |
| IT201 | Buckler / Giant Buckler / Large Buckler | gequip.s3d | — | classic only |
| IT202 | Shield / Kite Shield / Targ Shield | gequip.s3d | — | classic only |
| IT203 | Round Shield / Acorn Buckler / Large Round Shield | gequip.s3d | — | classic only |
| IT204 | Palocht / Shield of Ash / Darkwood Aegis | gequip.s3d | — | classic only |
| IT205 | Bone Shield / Scute Shield / Aegis of Life | gequip.s3d | — | classic only |
| IT206 | Bark Shield / Small Shield / Corroded Buckler | gequip.s3d | — | classic only |
| IT207 | Life's Guard / Devlas Ilkvel / Marr's Promise | gequip.s3d | — | classic only |
| IT208 | Buckler of Doom / Boar Hide Shield / Hopperhide Buckler | gequip.s3d | — | classic only |
| IT209 | Granite Buckler / Buckler of Insight / Buckler of Insanity | gequip.s3d | — | classic only |
| IT210 | Shield / Gounus Egg / Silver Disk | gequip.s3d | — | classic only |
| IT211 | Bladestopper / Slimy Shield / Guardian Shield | gequip.s3d | — | classic only |
| IT212 | Nautilus Shield / Ashenbone Shield / Halfling Ribcage | gequip.s3d | — | classic only |
| IT213 | Ragepaw Aegis / Death Bind Shield / Death Ward Shield | gequip.s3d | — | classic only |
| IT214 | Mind Surge Shield / Shield of Spiders / Chitin Shell Shield | gequip.s3d | — | classic only |
| IT215 | Reef Shield / Skull Shield / Shield of Fury | gequip.s3d | — | classic only |
| IT216 | Guard of Ik / Trooper Shield / Iksar Targ Shield | gequip.s3d | — | classic only |
| IT217 | Mossy Shield / Frogskin Shield / Frog Hide Shield | gequip.s3d | — | classic only |
| IT218 | Siren's Shield / Sugal's Shield / Drakescale Shield | gequip.s3d | — | classic only |
| IT219 | Shield of Elders / Aegis of the Shrine / Aged Shield of Elders | gequip.s3d | — | classic only |
| IT220 | Etched Chitin Shield / Shield of the Royals / Tae Ew Tribal Shield | gequip.s3d | — | classic only |
| IT221 | Legacy of Tyro / Atramentous Shield / Insignia Protector | gequip.s3d | — | classic only |
| IT222 | Shield of Auras / Shield of Ssraeshza / Ry`Gorr Oracle Shield | gequip.s3d | — | classic only |
| IT223 | Shield / Ornate Rune Shield / Raptor Hide Shield | gequip.s3d | — | classic only |
| IT224 | Sturdy Stone Shield / Aegis Guardian Shield / Intricate Defiant Hammered Buckler | gequip.s3d | — | classic only |
| IT225 | Barnacle-covered Shield | gequip.s3d | — | classic only |
| IT226 | Specter Shield / Shield of Battle / Shield of Falsehood | gequip.s3d | — | classic only |
| IT227 |  | gequip.s3d | — | classic only |
| IT228 | Aegis of Ice / Grintwisdom's Gift / Velium Round Shield | gequip2.s3d | — | classic only |
| IT250 | brd range / clr range / wiz range | gequip2.s3d | — | classic only |
| IT300 | Rujarkian Halberd / Black Acrylia Halberd / Grisly Halberd of War | gequip2.s3d | — | classic only |
| IT301 |  | gequip2.s3d | — | classic only |
| IT308 |  | gequip2.s3d | — | classic only |
| IT400 | Sticky Mud / Malleable Ooze / Small Dolerite Shard | gequip.s3d | — | classic only |
| IT401 | Pitaya / Dark Lady / Psilocybe | gequip.s3d | — | classic only |
| IT402 | Deepspore / Payala Fruit / Foraged Foods | gequip.s3d | — | classic only |
| IT403 | Oak Bark / Palm Leaf / Green Herbs | gequip.s3d | — | classic only |
| IT410 |  | gequip.s3d | — | classic only |
| IT411 |  | gequip.s3d | — | classic only |
| IT412 |  | gequip.s3d | — | classic only |
| IT530 |  | gequip2.s3d | — | classic only |
| IT535 |  | gequip2.s3d | — | classic only |
| IT536 | Unflinching Destruction | gequip2.s3d | — | classic only |
| IT537 | Crushed Helmet / Golden Helm of Rallos Zek | gequip2.s3d | — | classic only |
| IT540 | Crest of Fal`Kaa's Order | gequip2.s3d | — | classic only |
| IT545 |  | gequip2.s3d | — | classic only |
| IT550 |  | gequip2.s3d | — | classic only |
| IT555 |  | gequip2.s3d | — | classic only |
| IT556 | Dwarf Mining Helmet | gequip2.s3d | — | classic only |
| IT557 |  | gequip2.s3d | — | classic only |
| IT560 |  | gequip2.s3d | — | classic only |
| IT561 |  | gequip2.s3d | — | classic only |
| IT565 |  | gequip2.s3d | — | classic only |
| IT566 | Crest of Erion's Order | gequip2.s3d | — | classic only |
| IT570 |  | gequip2.s3d | — | classic only |
| IT575 |  | gequip2.s3d | — | classic only |
| IT580 |  | gequip2.s3d | — | classic only |
| IT585 |  | gequip2.s3d | — | classic only |
| IT590 |  | gequip2.s3d | — | classic only |
| IT595 |  | gequip2.s3d | — | classic only |
| IT600 |  | gequip2.s3d | — | classic only |
| IT605 |  | gequip2.s3d | — | classic only |
| IT610 |  | gequip2.s3d | — | classic only |
| IT615 |  | gequip2.s3d | — | classic only |
| IT620 |  | gequip2.s3d | — | classic only |
| IT625 |  | gequip2.s3d | — | classic only |
| IT626 |  | gequip2.s3d | — | classic only |
| IT627 |  | gequip2.s3d | — | classic only |
| IT628 |  | gequip2.s3d | — | classic only |
| IT629 |  | gequip2.s3d | — | classic only |
| IT630 |  | gequip2.s3d | — | classic only |
| IT635 |  | gequip2.s3d | — | classic only |
| IT640 |  | gequip2.s3d | — | classic only |
| IT641 |  | gequip2.s3d | — | classic only |
| IT645 |  | gequip2.s3d | — | classic only |
| IT646 | Worn Kromtus Helm | gequip2.s3d | — | classic only |
| IT650 |  | gequip2.s3d | — | classic only |
| IT655 | Bunch of Gnawing Grubs | gequip2.s3d | — | classic only |
| IT656 |  | gequip2.s3d | — | classic only |
| IT661 |  | gequip6.s3d | — | modern .s3d only |
| IT662 |  | gequip6.s3d | — | modern .s3d only |
| IT663 |  | gequip6.s3d | — | modern .s3d only |
| IT666 |  | gequip6.s3d | — | modern .s3d only |
| IT667 |  | gequip6.s3d | — | modern .s3d only |
| IT668 |  | gequip6.s3d | — | modern .s3d only |
| IT1036 |  | gequip.s3d | — | classic only |
| IT10000 | Sabertooth / Ember Blade / Slated Sword | gequip3.s3d | — | modern .s3d only |
| IT10001 | Goldenrod / Aggression / Mournfollow | gequip3.s3d | — | modern .s3d only |
| IT10002 | Armsman Blade / Hymnist Blade / Charred Scimitar | gequip3.s3d | — | modern .s3d only |
| IT10003 | Stone Hammer / Primal Cudgel / Righteousness | gequip3.s3d | — | modern .s3d only |
| IT10004 | Blindfoe / Soulbane / Ashbringer | gequip3.s3d | — | modern .s3d only |
| IT10005 | Norge`tal / Bronze Axe / Copper Axe | gequip3.s3d | — | modern .s3d only |
| IT10006 | Khalshazar / Vius dagger / Windspeeder | gequip3.s3d | — | modern .s3d only |
| IT10007 | Theurgist / Lunar mace / Spiny Mace | gequip3.s3d | — | modern .s3d only |
| IT10008 | Gemmed Maul / Golden Maul / Velium Maul | gequip3.s3d | — | modern .s3d only |
| IT10009 | Spiny Dirk / Grim Dagger / Flaming Dirk | gequip3.s3d | — | modern .s3d only |
| IT10010 | Fleshgrinder / Lost Fleshgrinder / Narikor Slave Axe | gequip3.s3d | — | modern .s3d only |
| IT10011 | Jagged Stone Axe / Savage Stone Axe / Strength of Grodan | gequip3.s3d | — | modern .s3d only |
| IT10012 | Flint Hammer / Bogling Mallet / Driftwood Club | gequip3.s3d | — | modern .s3d only |
| IT10013 | Goranga Warhammer / Sambata Warhammer / Pitch Coated Warclub | gequip3.s3d | — | modern .s3d only |
| IT10014 | Kyv Chitin Sword / Savage Boneblade / Elder Wolf Jawbone | gequip3.s3d | — | modern .s3d only |
| IT10015 | Claw of the Savage Spirit | gequip4.s3d | — | modern .s3d only |
| IT10016 | Bloodclaw / Kyv Chitin Shard / War Forged Shank | gequip3.s3d | — | modern .s3d only |
| IT10017 | Darkmoon Blade / Charred Broad Sword / Sword of the Damned | gequip3.s3d | — | modern .s3d only |
| IT10018 | Yttrium Falchion / Longsword of Freedom / Fury Edged Short Sword | gequip3.s3d | — | modern .s3d only |
| IT10019 | Bloodfrenzy / Chitin Blade / Poisoned Blade | gequip3.s3d | — | modern .s3d only |
| IT10020 | Vius mace / Skull Maul / Skybreaker | gequip3.s3d | — | modern .s3d only |
| IT10021 | Blackout / Hategiver / Deathshead | gequip3.s3d | — | modern .s3d only |
| IT10022 | Dull Energeiac Sword / Legendary Steel Sword / Glowing Energeiac Sword | gequip3.s3d | — | modern .s3d only |
| IT10023 | Ichorflow / Spiny Scimitar / Ancient Gladius | gequip3.s3d | — | modern .s3d only |
| IT10024 | Toetwister / Vius short sword / Lunar short sword | gequip3.s3d | — | modern .s3d only |
| IT10025 | Sword of Truth / Fabled Sword of Truth / Dark and Twisted Sword | gequip3.s3d | — | modern .s3d only |
| IT10026 | Sword of Ruin / Dark Steel Short Sword / Greatsword of Lost Souls | gequip4.s3d | — | modern .s3d only |
| IT10027 | Killing Pick / Black Acrylia Pick / Murph's Minin' Pick | gequip4.s3d | — | modern .s3d only |
| IT10028 | Corrupted Soul Piercer / Heavy Windriders Lance / Kavilis' Mosquito Lance | gequip4.s3d | — | modern .s3d only |
| IT10029 | Claw of the Savage Spirit / Primary Claw of the Savage Spirit Ornamentation | gequip4.s3d | — | modern .s3d only |
| IT10100 | Slook / Spear / Longfang | gequip3.s3d | — | modern .s3d only |
| IT10101 | Sharpened Wood Spear / Stave of the Slavers / Spear of the Dar Khura | gequip3.s3d | — | modern .s3d only |
| IT10103 | Blood Crusted Stake / Bogling Battlespear / Blessed Wooden Stake | gequip3.s3d | — | modern .s3d only |
| IT10104 | Carved Stabbing Spear / Splintering Oak Lance / Johanius Stake of Slaying | gequip3.s3d | — | modern .s3d only |
| IT10105 | Ishinaear Xiall / Seetheker's Spear / Black Acrylia Spear | gequip3.s3d | — | modern .s3d only |
| IT10106 | Sharp Stick / Flint Head Spear / Bogling War spear | gequip3.s3d | — | modern .s3d only |
| IT10107 | Bogling Spear / Spear of Rock / Ukun-Spine Spike | gequip3.s3d | — | modern .s3d only |
| IT10108 | Centi Warspear / Crude Bone Halberd / Spear of Foresight | gequip3.s3d | — | modern .s3d only |
| IT10109 | Crude Bone Spear / Ensorcelled Spear / Hardened Bone Spear | gequip3.s3d | — | modern .s3d only |
| IT10200 | Iron Staff / Rusty Staff / Bronze Staff | gequip3.s3d | — | modern .s3d only |
| IT10201 | Stave of Wit / Rod of Virtue / Staff of Matter | gequip3.s3d | — | modern .s3d only |
| IT10202 | Vius staff / Lunar staff / Astral staff | gequip3.s3d | — | modern .s3d only |
| IT10203 | Charred Rune Staff / Staff of the Midst / Dark Steel Great Staff | gequip4.s3d | — | modern .s3d only |
| IT10300 | Bow of Doom / Shissar Bow / Dark Courage | gequip3.s3d | — | modern .s3d only |
| IT10301 | Dread Dartgun / Chipped Bonedart / Carved Rod of Loathing | gequip3.s3d | — | modern .s3d only |
| IT10400 | SummonedTribunalHammer | gequip5.s3d | — | modern .s3d only |
| IT10401 |  | gequip5.s3d | — | modern .s3d only |
| IT10402 |  | gequip5.s3d | — | modern .s3d only |
| IT10403 |  | gequip5.s3d | — | modern .s3d only |
| IT10404 | Heartlight | gequip5.s3d | — | modern .s3d only |
| IT10405 | Chronomagic Axe / Weighty Polearm / Axe of Destruction | gequip5.s3d | — | modern .s3d only |
| IT10406 |  | gequip5.s3d | — | modern .s3d only |
| IT10407 | Greatstaff of Power / Staff of the Hamahiru / Polymorph Wand: Phantom Froglok | gequip5.s3d | — | modern .s3d only |
| IT10408 |  | gequip5.s3d | — | modern .s3d only |
| IT10409 | Petamorph Wand - Nightmare Skull | gequip5.s3d | — | modern .s3d only |
| IT10410 | Spear of Fire / Time's Antithesis / Fabled Spear of Fire | gequip5.s3d | — | modern .s3d only |
| IT10411 | Darwol's Cleaver / Cleaver of Vengence / Servant's Bloodsword | gequip5.s3d | — | modern .s3d only |
| IT10412 | Kyv Bow / The Barb / Black Insanity | gequip5.s3d | — | modern .s3d only |
| IT10413 | Obsidian Scimitar of War / Baros, Scimitar of Vallon Zek / Tactician's Obsidian Scimitar | gequip5.s3d | — | modern .s3d only |
| IT10501 | Torch of Lxvanvom / Crook of the Cursed / Caduceus of the Deep | gequip5.s3d | — | modern .s3d only |
| IT10502 | Wand of Magma / Drachnid Candle / Sacrificer's Wand | gequip5.s3d | — | modern .s3d only |
| IT10503 | Shadowdrake Orb / Dull Energeiac Rod / Fieldstrider Brand | gequip5.s3d | — | modern .s3d only |
| IT10504 | Sparkthorn / Wand of Mal / Relic of Mal | gequip5.s3d | — | modern .s3d only |
| IT10505 | Darksoul Staff / Crystallized Mind / Darkscreamer Staff | gequip5.s3d | — | modern .s3d only |
| IT10506 | Fiery Staff of Zha / Plasmatic Firestaff / Signal Fire Scepter | gequip5.s3d | — | modern .s3d only |
| IT10507 | Club of Slime / Gorewood Staff / Star Reaver Staff | gequip5.s3d | — | modern .s3d only |
| IT10508 | Screamstaff / Steel Staff / Andak's Staff | gequip5.s3d | — | modern .s3d only |
| IT10509 | Fairy Wing / Lifebinder / Onyx Sphere | gequip5.s3d | — | modern .s3d only |
| IT10510 | Ent Leaf / Rathebark / Pulsing Goo | gequip5.s3d | — | modern .s3d only |
| IT10511 | Eye of Ice / Baneice Orb / Misty Globe | gequip5.s3d | — | modern .s3d only |
| IT10512 | Manastone / Air Fetish / Guja Token | gequip5.s3d | — | modern .s3d only |
| IT10513 | Mindtaker / Blood Defiance / Warscarred Cutlass | gequip5.s3d | — | modern .s3d only |
| IT10514 | Paragon's Blade / Cavalry Short Sword / Golden Blood-Drinker | gequip5.s3d | — | modern .s3d only |
| IT10515 | Hopebringer / Ornate Broadsword / Charger's Hip Blade | gequip5.s3d | — | modern .s3d only |
| IT10516 | Razornails / Fippy's Paw / Curved Claws | gequip5.s3d | — | modern .s3d only |
| IT10517 | Goad of the Just / The Rotting Fist / Ethereal Destroyer | gequip5.s3d | — | modern .s3d only |
| IT10518 | Crushing Demolisher / War Forged Warhammer / Mithril Battle Hammer | gequip5.s3d | — | modern .s3d only |
| IT10519 | Krossmaul / Toe Smasher / Corpsegrinder | gequip5.s3d | — | modern .s3d only |
| IT10520 | Drachnid Cane / Charred Wooden Rod / Mithril Shod Staff | gequip5.s3d | — | modern .s3d only |
| IT10521 | Wind Thistle / Stick of Fury / Armsman's Staff | gequip5.s3d | — | modern .s3d only |
| IT10522 | Crystal Hilted Shinai / Shinai of the Ancients / Shinai of the Ancients Ornamentation | gequip5.s3d | — | modern .s3d only |
| IT10523 | Rakusha Bo / Fist of Force / Will of Zan Fi | gequip5.s3d | — | modern .s3d only |
| IT10524 | Icefloe Hammer / Hammer of Hours / Jagged Coral Sledge | gequip5.s3d, gequip8.s3d | — | REPLACED (later .s3d) |
| IT10525 | Crystal Web Mace / Deep Forest Mace / Kelp Hilted Mace | gequip5.s3d | — | modern .s3d only |
| IT10526 | God Ender / Wicked Shank / Knife of Justice | gequip5.s3d | — | modern .s3d only |
| IT10527 | Father's Knife / Paragon's Dirk / Lunatic's Blade | gequip5.s3d | — | modern .s3d only |
| IT10528 | Obsidian Dirk / Chailak's Fang / Thelin's Dagger | gequip5.s3d | — | modern .s3d only |
| IT10530 | Truthward, Guardian of Light / Mithaniel Marr Shield Ornament | gequip5.s3d | — | modern .s3d only |
| IT10531 | Warborn Shield / Shield of Strife / Drachnid Sentry Shield | gequip5.s3d | — | modern .s3d only |
| IT10532 | Mound of Living Stone / Shell of the Cliffwalker / The Rathe Shield Ornament | gequip5.s3d | — | modern .s3d only |
| IT10533 | Saryrn Council Shield Ornament | gequip5.s3d | — | modern .s3d only |
| IT10534 | Tribunal Council Shield Ornament | gequip5.s3d | — | modern .s3d only |
| IT10535 | Tactician's Shield / Hoplon of the Tactician / Fabled Tactician's Shield | gequip5.s3d | — | modern .s3d only |
| IT10536 | Wall of Terror / Shadowmane Aegis / Nightmarish Shield | gequip5.s3d | — | modern .s3d only |
| IT10537 | Guard of the Strategos / Vallon Zek Shield Ornament | gequip5.s3d | — | modern .s3d only |
| IT10538 | Amorphous Cloud of Air / Xegony Shield Ornament | gequip5.s3d | — | modern .s3d only |
| IT10539 | Bertoxxulous Shield Ornament | gequip5.s3d | — | modern .s3d only |
| IT10540 | Aegis of the Valiant / Karana Shield Ornament | gequip5.s3d | — | modern .s3d only |
| IT10541 | Zebuxoruk Shield Ornament | gequip5.s3d | — | modern .s3d only |
| IT10542 | Sphere of Coalesced Water / Whorl of Unnatural Forces / Tarew Marr Shield Ornament | gequip5.s3d | — | modern .s3d only |
| IT10543 | Fiery Crystal Guard / Globe of Dancing Flame / Fennin Ro Shield Ornament | gequip5.s3d | — | modern .s3d only |
| IT10544 | Blood Defender | gequip5.s3d | — | modern .s3d only |
| IT10545 | Edge of Eternity / Murderer's Blade / Rune-Covered Shiv | gequip5.s3d | — | modern .s3d only |
| IT10600 | Coral Horn / Queen's Horn / Enchanted Horn | gequip5.s3d | — | modern .s3d only |
| IT10601 | Flute / Bone Fife / Faun Flute | gequip5.s3d | — | modern .s3d only |
| IT10602 | Deepbelly / Selo's Drum / Darksong Gem | gequip5.s3d | — | modern .s3d only |
| IT10603 | Swansong / Lisera Lute / Aged War Lute | gequip5.s3d | — | modern .s3d only |
| IT10604 | Truthmaker / Serrated Mace / Coral Headed Mace | gequip5.s3d | — | modern .s3d only |
| IT10605 | Entwood Mace / Mace of Dark Glory / Steelcrown War Mace | gequip5.s3d | — | modern .s3d only |
| IT10606 | Troubadour's Mace / Fallen Froglok's Scepter / Flamekissed Scepter of War | gequip5.s3d | — | modern .s3d only |
| IT10607 | Grandmasters Mace / Mace of Regression / Narikor Slave Mace | gequip5.s3d | — | modern .s3d only |
| IT10608 | Flail / Evensong / Ebon Mace | gequip5.s3d | — | modern .s3d only |
| IT10609 | Tempest Mace / Cudgel of Wrecking / Mace of the Ancients | gequip5.s3d | — | modern .s3d only |
| IT10610 | Gomdurig / Inured Dagger / Ornate Dagger | gequip5.s3d | — | modern .s3d only |
| IT10611 | Sphere of Ice I / Sphere of Ice V / Sphere of Ice II | gequip5.s3d | — | modern .s3d only |
| IT10612 | Wind Lance / Castrum Lance / War Rage Lance | gequip5.s3d | — | modern .s3d only |
| IT10613 | Garduk / Naraithus / Runed Spear | gequip5.s3d | — | modern .s3d only |
| IT10614 | Bow / Longbow / Oakwynd | gequip5.s3d | — | modern .s3d only |
| IT10615 | Windblade / Fleshripper / Ervonis's Axe | gequip5.s3d | — | modern .s3d only |
| IT10616 | Diaku Forged Sword / War Forged Longsword / Sullied Mithril Blade | gequip5.s3d | — | modern .s3d only |
| IT10617 | Eshen Blade / Rune Stamped Blade / Charger Backup Sword | gequip5.s3d | — | modern .s3d only |
| IT10618 | Paragon's Dagger / Slaver's Falchion / Flame Etched Dagger | gequip5.s3d | — | modern .s3d only |
| IT10619 | Cold Steel Brand / Blackend Greatsword / Greatblade of Chaos | gequip5.s3d | — | modern .s3d only |
| IT10620 | Dark Whisperer / Ornate Shortsword / Shortsword of Woe | gequip5.s3d | — | modern .s3d only |
| IT10621 | Soulpiercer / Gargoyle Talon / Castrum Ritual Dagger | gequip5.s3d | — | modern .s3d only |
| IT10622 | War Forged Scimitar / Diaku Forged Scimitar / Tendon-Hilted Scimitar | gequip5.s3d | — | modern .s3d only |
| IT10623 | Sword of Wars / Jagged Blade of Ice / Wrought Blade of Hiz | gequip5.s3d | — | modern .s3d only |
| IT10624 | Flametongue / Runebladed Dagger / Blood Bladed Dagger | gequip5.s3d | — | modern .s3d only |
| IT10625 | Tuna / Herring / Pot of Gold | gequip5.s3d | — | modern .s3d only |
| IT10626 | Decorin Ore Shard / Spider Fang Sword / Ivory Hilted Cleaver | gequip5.s3d | — | modern .s3d only |
| IT10627 | Blood Fire Signet / Deep Forest Blade | gequip5.s3d | — | modern .s3d only |
| IT10628 | Mariner's Falchion / Bone-Wrapped Falchion / Timeless Coral Greatsword | gequip5.s3d | — | modern .s3d only |
| IT10629 | Falchion of Storms / Jade Scimitar of Turina / Sword of Midwinter's Vengeance | gequip5.s3d | — | modern .s3d only |
| IT10630 | Armsman's Shiv / The Putrid Fish / Polymorph Wand: Kedge | gequip5.s3d | — | modern .s3d only |
| IT10631 | Couragebringer / Slimy Longsword / Guktan Defenders Sword | gequip5.s3d | — | modern .s3d only |
| IT10632 | Soulsteel Blade / Slimy Shortsword / Darkore Shortblade | gequip5.s3d | — | modern .s3d only |
| IT10633 | Sharp Gem Formation / Corathus Edged Dagger / Dagger of Distraction | gequip5.s3d | — | modern .s3d only |
| IT10634 | Sporali Mace / Ethermasher Maul / Shard of Grokui's Claw | gequip5.s3d | — | modern .s3d only |
| IT10635 | Entwined Hammer / Heavy Bludgeoner / Mace of Savagery | gequip5.s3d | — | modern .s3d only |
| IT10636 | Black Insanity / War Forged Gavel / Bloodforge Hammer | gequip5.s3d | — | modern .s3d only |
| IT10637 | Onyx Gavel / Dark Courage / Inured Hammer | gequip5.s3d | — | modern .s3d only |
| IT10638 | Pile Driver / Anklesmasher / Bed Leg Club | gequip5.s3d | — | modern .s3d only |
| IT10639 | Satyr Legbone / Dreadwood Maul / Healing Hammer | gequip5.s3d | — | modern .s3d only |
| IT10640 | Ornate Greatblade / Paragon's Claymore / Corathus Long Sword | gequip5.s3d | — | modern .s3d only |
| IT10641 | Bow of Supremacy / Bow of the Elddar / Bow of Eternal Ice | gequip5.s3d | — | modern .s3d only |
| IT10642 | Eye of Chailak / Spore-Covered Rod / Wand of Blue Fire | gequip5.s3d | — | modern .s3d only |
| IT10643 | Draconian Idol / Shrieking Staff / Witchwood Staff | gequip5.s3d | — | modern .s3d only |
| IT10644 | Shifting Glaive / Axe of Slaughter / Greatstaff of Misery | gequip5.s3d | — | modern .s3d only |
| IT10645 | Lexicon / Worn Book / Legacy Log | gequip5.s3d | — | modern .s3d only |
| IT10646 | Spirit Tome / Ancient Text / Book of Dawn | gequip5.s3d | — | modern .s3d only |
| IT10647 | Allin's Mace / Gnollcracker / Sharkspine Club | gequip5.s3d | — | modern .s3d only |
| IT10648 | Claymore / Truvinan / Ghoulbane | gequip5.s3d | — | modern .s3d only |
| IT10649 | Long Sword / Short Sword / Dark Courage | gequip5.s3d | — | modern .s3d only |
| IT10650 | Seax / Dagger / Pugius | gequip5.s3d | — | modern .s3d only |
| IT10651 | Black Insanity / Gemmed Great Sword / Golden Great Sword | gequip5.s3d | — | modern .s3d only |
| IT10652 | Dagas / Yannikil / Icy Blade | gequip5.s3d | — | modern .s3d only |
| IT10653 | Woe / Gladius / Langseax | gequip5.s3d | — | modern .s3d only |
| IT10654 | Gnollcleaver / Masterforged Claymore / Blade of the Full Moon | gequip5.s3d | — | modern .s3d only |
| IT10655 | Burrow's Edge / Adjutant's Saber / Tardunk's Poniard | gequip5.s3d | — | modern .s3d only |
| IT10656 | Gnollbiter / Bonehilt Dagger / Flawed Defiant Dagger | gequip5.s3d | — | modern .s3d only |
| IT10657 | Blade of War / Fabled Blade of War / Gleaming Blade of Elddar | gequip5.s3d | — | modern .s3d only |
| IT10658 | Darkblade of the Warlord / Scimitar of the Earthmarch / Darkblade of the Warlord Ornamentation | gequip5.s3d | — | modern .s3d only |
| IT10659 | Bejeweled Dagger of Summoning / Fabled Bejeweled Dagger of Summoning | gequip5.s3d | — | modern .s3d only |
| IT10660 | Coral Hilted Tulwar / Thundercrest Katana / Great Blade of Heroism | gequip5.s3d | — | modern .s3d only |
| IT10661 | Malarian Wing Blade / Blade of the Tempest / Summoned: Blade of Walnan | gequip5.s3d | — | modern .s3d only |
| IT10662 |  | gequip5.s3d | — | modern .s3d only |
| IT10663 | Castrum Jeweled Wand / Polymorph Wand: Siren Sorceress / Xephyrus, Wand of the Flowing Wind | gequip5.s3d | — | modern .s3d only |
| IT10664 | Hero's Aegis / Shield of Fire / Aegis of Yar`Lir | gequip5.s3d | — | modern .s3d only |
| IT10665 | Hero's Heater / Reaper Chest Plate / Bloodstained Mirror | gequip5.s3d | — | modern .s3d only |
| IT10666 | Shabby Lobby Door / Bulwark of Many Portals | gequip5.s3d | — | modern .s3d only |
| IT10667 |  | gequip5.s3d | — | modern .s3d only |
| IT10668 | Bladestopper / Windhowl Shield / Armsman's Shield | gequip5.s3d | — | modern .s3d only |
| IT10669 | Paragon's Targe / Armsman's Buckler / Brightstar Buckler | gequip5.s3d | — | modern .s3d only |
| IT10670 | Basalt Bulwark / Shield of Malaise / Drachnid Preserver | gequip5.s3d | — | modern .s3d only |
| IT10671 | Aegis of Gloom / Dagda's Shield / Paragon's Aegis | gequip5.s3d | — | modern .s3d only |
| IT10733 | Staff of Endless Adventure / Marnek's Wand of the Burning Dead | gequip8.s3d | — | modern .s3d only |
| IT10735 | Blade of Vesagran / Blade of Vesagran Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10736 | Spiritcaller Totem of the Feral / Spiritcaller Totem of the Feral Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10737 | Vengeful Taelosian Blood Axe / Vengeful Taelosian Blood Axe Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10738 | Aegis of Superior Divinity / Aegis of Superior Divinity Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10739 | Staff of Everliving Brambles / Staff of Everliving Brambles Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10740 | Staff of Eternal Eloquence / Staff of Eternal Eloquence Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10741 | Focus of Primal Elements / Focus of Primal Elements Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10742 | Transcended Fistwraps of Immortality / Transcended Fistwraps of Immortality Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10743 | Deathwhisper / Deathwhisper Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10744 | Nightbane, Sword of the Valiant / Nightbane, Sword of the Valiant Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10745 | Aurora, the Heartwood Blade / Aurora, the Heartwood Blade Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10746 | Nightshade, Blade of Entropy / Nightshade, Blade of Entropy Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10747 | Innoruuk's Dark Blessing / Innoruuk's Dark Blessing Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10748 | Blessed Spiritstaff of the Heyokah / Blessed Spiritstaff of the Heyokah Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10749 | Kreljnok's Sword of Eternal Power / Kreljnok's Sword of Eternal Power Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10750 | Staff of Phenomenal Power / Unpowered Staff of Phenomenal Power / Staff of Phenomenal Power Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10751 | Prismatic Dragon Blade / Prismatic Dragon Blade Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10752 | Savage Lord's Totem / Savage Lord's Totem Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10753 | Raging Taelosian Alloy Axe / Raging Taelosian Alloy Axe Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10754 | Harmony of the Soul / Harmony of the Soul Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10755 | Staff of Living Brambles / Staff of Living Brambles Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10756 | Oculus of Persuasion / Oculus of Persuasion Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10757 | Staff of Elemental Essence / Staff of Elemental Essence Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10758 | Fistwraps of Celestial Discipline / Fistwraps of Celestial Discipline Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10759 | Soulwhisper / Soulwhisper Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10760 | Redemption / Redemption Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10761 | Heartwood Blade / Heartwood Blade Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10762 | Fatestealer / Wren's Fatestealer / Fatestealer Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10763 | Innoruuk's Voice / Innoruuk's Voice Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10764 | Crafted Talisman of Fates / Crafted Talisman of Fates Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10765 | Raging Sword of Eternal Power / Champion's Sword of Eternal Power / Champion's Sword of Eternal Power Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10766 | Staff of Prismatic Power / Staff of Prismatic Power Ornamentation | — | omensequip.eqg | eqg-only (new item) |
| IT10767 | Wand of Defiance / Fizzlepop Atomizer / The Eye of Despair | — | omensequip.eqg | eqg-only (new item) |
| IT10768 | Zomm's Wand / Hero's Dragonrod / Magic Dream Nexus | — | omensequip.eqg | eqg-only (new item) |
| IT10769 | Soulskive / Crystal Dagger / Fang of Bloodlust | — | omensequip.eqg | eqg-only (new item) |
| IT10770 | Fang of Feratha / Sicklesting Blade / Sebilisian Longsword | — | omensequip.eqg | eqg-only (new item) |
| IT10771 | Iris Blade / Morguecaller / Grelian Cleaver | — | omensequip.eqg | eqg-only (new item) |
| IT10772 | Xixkt's Shield / Kromrif Buckler / Attendant's Shield | — | omensequip.eqg | eqg-only (new item) |
| IT10773 | Perilous Pike / Hero's Poleaxe / Battle Deep Spear | — | omensequip.eqg | eqg-only (new item) |
| IT10774 | Dragorn Greatstaff / Rod of Shaded Wrath / Disciple's Dragonstaff | — | omensequip.eqg | eqg-only (new item) |
| IT10775 | Illsalin Templar Shield / Aegis of the Dragorn Elders / Crysnos, Shield of the Rift | — | omensequip.eqg | eqg-only (new item) |
| IT10776 | Bonecleaver / Hero's Claymore / Dark Crystal Blade | — | omensequip.eqg | eqg-only (new item) |
| IT10777 | Despair / Shademaul / Barraki Torture Club | — | omensequip.eqg | eqg-only (new item) |
| IT10778 | Kobold Whip / Thunderclap / Eight-loop Lasso | — | omensequip.eqg | eqg-only (new item) |
| IT10779 | Painblade / Blade of Forgotten Faith / Longsword of the Bridgekeeper | — | omensequip.eqg | eqg-only (new item) |
| IT10780 | Razor-Sharp Speedblade / Blade of the Last Knight / Obsidian Scale Wakizashi Ornamentation | — | donequip.eqg | eqg-only (new item) |
| IT10781 | Stormcrest / Crescent Palemoon / Buckler of Unliving | — | donequip.eqg | eqg-only (new item) |
| IT10782 | Paragon's Spear / Galeth's Greatlance / Great Lance of the Castrum | — | donequip.eqg | eqg-only (new item) |
| IT10783 | Icecracker / Bleakwood Tonfa / Cryptwood Tonfa | — | donequip.eqg | eqg-only (new item) |
| IT10784 | Hero's Kama / Howling Hatchet / Elaborate Defiant Kama | — | donequip.eqg | eqg-only (new item) |
| IT10785 | Painbringer / Crystalline Blade / Faded Crystalline Blade | — | donequip.eqg | eqg-only (new item) |
| IT10786 | Deadclaw / Polished Sunsword / Big Bladed Greatsword | — | donequip.eqg | eqg-only (new item) |
| IT10787 | Dragonkiller / Devlin's Whip / Vampire Killer | — | donequip.eqg | eqg-only (new item) |
| IT10788 | Wand of Fury / Orb of Dark Water / Wrapped Glass Shard | — | donequip.eqg | eqg-only (new item) |
| IT10789 | Deathwind / Enforcer's Bow / Bound Torsion Spring | — | donequip.eqg | eqg-only (new item) |
| IT10790 | Ykesha's Shield / Shield of the Pyre / Skull of Vishimtar | — | donequip.eqg | eqg-only (new item) |
| IT10791 | Ebony Bladed Sword / Blade of the Eclipse / Obsidian Scale Longsword Ornamentation | — | donequip.eqg | eqg-only (new item) |
| IT10792 | Rolth's Aerocleave / Ashen Blade of the Slayer / Blade of the Newborn Vampire | — | donequip.eqg | eqg-only (new item) |
| IT10793 | Stix's Blade / Elegant Defiant Nightblade / Elegant Consigned Nightblade | — | donequip.eqg | eqg-only (new item) |
| IT10794 | Bloodclaw Staff / Staff of Ankexfen / Steam Forged Iron Rod | — | donequip.eqg | eqg-only (new item) |
| IT10795 | Hammer of Shadows / Kraken Claw Hammer / Grokui's Barbed Claw | — | donequip.eqg | eqg-only (new item) |
| IT10796 | Clawhammer / Kraken Claw Maul / Great Hammer of Respite | — | donequip.eqg | eqg-only (new item) |
| IT10797 | Windfury Katana / Gemmed Ceremonial Katana / Obsidian Scale Katana Ornamentation | — | donequip.eqg | eqg-only (new item) |
| IT10798 | Fang of Kessdona / Harith's Ornament / Death's Maw Dagger | — | donequip.eqg | eqg-only (new item) |
| IT10799 | The Keen Edge of Discord / Obsidian Scale Curved Stiletto Ornamentation / Unfired Obsidian Scale Curved Stiletto Ornament | — | donequip.eqg | eqg-only (new item) |
| IT10806 | Bloodred Ink / Glowing Solvent / Stored Medicine | — | donequip.eqg | eqg-only (new item) |
| IT10807 | Beast Poison / Poisoned Water / Analytic Potion | — | donequip.eqg | eqg-only (new item) |
| IT10808 | Murky Potion / Foul Fish Wine / Vile Substance | — | donequip.eqg | eqg-only (new item) |
| IT10810 | Erilynne, the Master's Mistress | — | dodequip.eqg | eqg-only (new item) |
| IT10811 | Therigal's Sword / Blade of the Cursed / Discordling Shoulderspike | — | dodequip.eqg | eqg-only (new item) |
| IT10812 | Dalur Ak-Daal / Valen Ak-Daal / Sunfire, Vampire Slayer | — | dodequip.eqg | eqg-only (new item) |
| IT10813 | Sacrificial Dagger / Lilac, Cruel Manipulator / Serpentine Ornate Backstabber | — | dodequip.eqg | eqg-only (new item) |
| IT10814 | Jagged Silver Shortsword | — | dodequip.eqg | eqg-only (new item) |
| IT10815 | Throatslitter / Dagger of Blazing Fires / Sebilisian Dagger of Blood | — | dodequip.eqg | eqg-only (new item) |
| IT10816 |  | — | dodequip.eqg | eqg-only (new item) |
| IT10817 | Spiked Cudgel of Rage / Staff of the Invaders / Corrupt Defender's Mace | — | dodequip.eqg | eqg-only (new item) |
| IT10818 | Biter / Thunderspike Maul / Cryptwood Truncheon | — | dodequip.eqg | eqg-only (new item) |
| IT10819 | Enspelled Wand / Call of Clan Vorzek / Staff of the Confidant | — | dodequip.eqg | eqg-only (new item) |
| IT10820 | Grisly Fetish / Rod of the Ruined / Staff of Feral Agony | — | dodequip.eqg | eqg-only (new item) |
| IT10821 | Dark Master's Blade / Erilynne Ornamentation | — | dodequip.eqg | eqg-only (new item) |
| IT10822 | Sunstone Wand / Icon of the Deep / Truncheon of Blood | — | dodequip.eqg | eqg-only (new item) |
| IT10823 | Gate Caller's Battlestaff / Wavecasters Ceremonial Staff / Staff of Arcane Dispossession | — | dodequip.eqg | eqg-only (new item) |
| IT10824 | Sister's Opulent Backstabber / Sister's Opulent Backstabber Ornamentation / Unfired Sister's Opulent Backstabber Ornament | — | dodequip.eqg | eqg-only (new item) |
| IT10825 | Swiftcleave, the Flesh Carver / Ticka, Greatblade of the Ravager / Swiftcleave, the Flesh Carver Ornamentation | — | dodequip.eqg | eqg-only (new item) |
| IT10826 | Devlin's Shield / Bulwark of Bravery / Buckler of Corruption | — | dodequip.eqg | eqg-only (new item) |
| IT10827 | Distorting Guard / Gaudy Demonhide Buckler / Resplendent Leather Aegis | — | dodequip.eqg | eqg-only (new item) |
| IT10828 | Lance of Balance / Elegant Defiant Forked Spear / Elegant Consigned Forked Spear | — | dodequip.eqg | eqg-only (new item) |
| IT10829 | Clockwork Scepter / Enhanced Arcsteel Mace / Skymaul the Magnificent | — | dodequip.eqg | eqg-only (new item) |
| IT10830 | Tempered Steamwound Pummeler | — | dodequip.eqg | eqg-only (new item) |
| IT10831 | Gnoll Rune Trap / Mineral Extractor / Calibration Device | — | dodequip.eqg | eqg-only (new item) |
| IT10832 | Impromptu Body Protector / Nogglegrop the Steadfast / Reinforced Steamwork Shield | — | dodequip.eqg | eqg-only (new item) |
| IT10833 | Ethernere Portal Device / Clockwork Defensive Guard / Steam-Powered Defensive Device | — | dodequip.eqg | eqg-only (new item) |
| IT10834 | Rod of Dimensional Imprisonment / Petamorph Wand - Steamwork Soldier | — | dodequip.eqg | eqg-only (new item) |
| IT10835 | Clockwork Infantry Bayonet | — | dodequip.eqg | eqg-only (new item) |
| IT10836 | Summoned: Exigent Minion / Ancient Sigil-Runed Censer / Scepter of the Illusionist | — | dodequip.eqg | eqg-only (new item) |
| IT10837 | Rendsoul / Paragon's Scepter / Moonstone Mind Scepter | — | dodequip.eqg | eqg-only (new item) |
| IT10838 | Twinfang, the Immortal's Bane | — | dodequip.eqg | eqg-only (new item) |
| IT10839 | Cragmaker / Overmace of Righteous Rathe | — | dodequip.eqg | eqg-only (new item) |
| IT10840 | The Shattered Blade / Sword of Frozen Wind / Shiliskin Battle Deep Blade | — | dodequip.eqg | eqg-only (new item) |
| IT10841 | Tideslasher / Wavethrasher / Runic Green Legion Longsword | — | dodequip.eqg | eqg-only (new item) |
| IT10842 | Wolfclaws / Hero's Quickclaw / Divine Fetters of Ro | — | dodequip.eqg | eqg-only (new item) |
| IT10843 | The Sweeper / Ceremonial Mop / Trelinna's Sweeper | — | porequip.eqg | eqg-only (new item) |
| IT10844 | Castrum Dagger / Elegant Defiant Dayblade / Elegant Consigned Dayblade | — | porequip.eqg | eqg-only (new item) |
| IT10845 | Crystalline Sledge / Cerulean Crystal Hammer / Extravagant Infused Crystalline Long Hammer | — | porequip.eqg | eqg-only (new item) |
| IT10846 | Smoldering Cudgel of Grief / Heor`Otor the Burning Waste | — | porequip.eqg | eqg-only (new item) |
| IT10847 | Kranigan's Pulverizer / Bloodied Skull Crusher | — | porequip.eqg | eqg-only (new item) |
| IT10848 | Scimitar of the Mistwalker / Faded Scimitar of the Mistwalker / Extravagant Infused Scimitar of Nature | — | porequip.eqg | eqg-only (new item) |
| IT10849 | Hearthshield / Turquoise Buckler / Deep-Hewn Crystal Bulwark | — | porequip.eqg | eqg-only (new item) |
| IT10850 | Azureguard / Temporal Field / Trexxt the White | — | porequip.eqg | eqg-only (new item) |
| IT10851 | Grave Shovel / Digging Tools / Marauder's Shovel | — | porequip.eqg | eqg-only (new item) |
| IT10852 | Forbidden Chord / Battlestaff of Symbols | — | porequip.eqg | eqg-only (new item) |
| IT10853 | Chataya's Staff / Haraibou of Screaming Souls / Wyvern-Priest's Runed Censer | — | porequip.eqg | eqg-only (new item) |
| IT10854 | Staff of Screaming Souls / Shattered Siren Staff Piece | — | porequip.eqg | eqg-only (new item) |
| IT10855 |  | — | porequip.eqg | eqg-only (new item) |
| IT10856 | Bloody Scythe / Scythe of Skeletal Expulsion / August Infused Bloodworked Reaper | — | porequip.eqg | eqg-only (new item) |
| IT10857 | Refraction Tower Shield / Bulwark of Stained Glass | — | porequip.eqg | eqg-only (new item) |
| IT10858 | Enigmatic Buckler / Last Treble Buckler | — | porequip.eqg | eqg-only (new item) |
| IT10859 | Skulltutor / Gurrak Bludgeoner | — | porequip.eqg | eqg-only (new item) |
| IT10860 | Feral Igneous Chopper / Elegant Defiant Fireborn Axe / Elegant Consigned Fireborn Axe | — | porequip.eqg | eqg-only (new item) |
| IT10861 | Brasse's Stein / Summoned: Party Mug / Mug of the Endless Fan Faire | — | porequip.eqg | eqg-only (new item) |
| IT10862 | Draconic Cane of the Everlasting / Coldain Pikestaff of the Steadfast / Kromzekian Walking Stick of Allegiance | — | porequip.eqg | eqg-only (new item) |
| IT10866 | Ripper / Steamwork Shiv | — | tssequip.eqg | eqg-only (new item) |
| IT10867 | Fateslayer | — | tssequip.eqg | eqg-only (new item) |
| IT10868 | Magma-ground Dirk / Mad Mephit's Blade / Embershank, the Malady | — | tssequip.eqg | eqg-only (new item) |
| IT10869 | Razors Edge / Prize: Razors Edge / Molten Blade of the Smelter | — | tssequip.eqg | eqg-only (new item) |
| IT10870 | Bale and Brimstone / Kindjal of Martyrdom / Element of Punishment | — | tssequip.eqg | eqg-only (new item) |
| IT10871 | Warspite / Highblade of the Wyvern Lord | — | tssequip.eqg | eqg-only (new item) |
| IT10872 | Stormsword / Satyr Sword / Mithril Two-Handed Sword | — | tssequip.eqg | eqg-only (new item) |
| IT10873 | Fieldstrider Lieutenant Sword | — | tssequip.eqg | eqg-only (new item) |
| IT10874 | Fieldstrider Gladius / Ogre Watchtower Blade | — | tssequip.eqg | eqg-only (new item) |
| IT10875 | Fieldstrider Sword / Blackburrow Claymore / Granite Blade of War | — | tssequip.eqg | eqg-only (new item) |
| IT10876 | Warsorrow / Warweaver | — | tssequip.eqg | eqg-only (new item) |
| IT10877 | Belgar's Blade / Inured Great Sword / Ornate Great Sword | — | tssequip.eqg | eqg-only (new item) |
| IT10878 | Broken Wraithguard Blade of Symbols / Frost Runed Blade of the Wraithguard | — | tssequip.eqg | eqg-only (new item) |
| IT10879 | Extravagant Infused Knight's Great Sword / Extravagant Uninfused Knight's Great Sword | — | tssequip.eqg | eqg-only (new item) |
| IT10880 | Coldrage Hateblade / Sekv, Blade of Glacial Fury | — | tssequip.eqg | eqg-only (new item) |
| IT10881 |  | — | tssequip.eqg | eqg-only (new item) |
| IT10882 | Sleet / Icy Dirk of Meditation / Tyranont Charged Spike | — | tssequip.eqg | eqg-only (new item) |
| IT10883 | Runed Longsword / Broken Krithgor Sword | — | tssequip.eqg | eqg-only (new item) |
| IT10884 | Sergeant's Longsword / Shattered Krithgor Blade / Ancient Kromtus Longsword | — | tssequip.eqg | eqg-only (new item) |
| IT10885 | Veinsplitter | — | tssequip.eqg | eqg-only (new item) |
| IT10886 |  | — | tssequip.eqg | eqg-only (new item) |
| IT10887 | Shadow Forged Blade / Shortblade of Prismatic Destruction | — | tssequip.eqg | eqg-only (new item) |
| IT10888 | Punctual Earthsaber | — | tssequip.eqg | eqg-only (new item) |
| IT10889 | Damaged Krithgor Hand Axe | — | tssequip.eqg | eqg-only (new item) |
| IT10890 | Grawlthin's Axe / Ruined Krithgor Axe / Silvery Two Handed Axe | — | tssequip.eqg | eqg-only (new item) |
| IT10891 | Axe of the Decimator / Axe of the Eradicator / Axe of the Annihilator | — | tssequip.eqg | eqg-only (new item) |
| IT10892 | Fellwinter | — | tssequip.eqg | eqg-only (new item) |
| IT10893 |  | — | tssequip.eqg | eqg-only (new item) |
| IT10894 | Extravagant Infused Glacial Great Axe / Extravagant Uninfused Glacial Great Axe | — | tssequip.eqg | eqg-only (new item) |
| IT10895 | Shadow Forged Axe | — | tssequip.eqg | eqg-only (new item) |
| IT10896 |  | — | tssequip.eqg | eqg-only (new item) |
| IT10897 | Drakkin Steel Longsword | — | tssequip.eqg | eqg-only (new item) |
| IT10898 | Drakkin Steel 2h Sword | — | tssequip.eqg | eqg-only (new item) |
| IT10899 | Weighty Jagged Einhander | — | tssequip.eqg | eqg-only (new item) |
| IT10900 |  | — | tssequip.eqg | eqg-only (new item) |
| IT10901 | Feareater / Summoned: Jagged Ragesword | — | tssequip.eqg | eqg-only (new item) |
| IT10902 | Dyn`leth Steel 2h Sword | — | tssequip.eqg | eqg-only (new item) |
| IT10903 | Bixie Soldier's Greatsword | — | tssequip.eqg | eqg-only (new item) |
| IT10904 | Axe of Vine Hewing / Drakkin Steel 1h Axe / Arcekor, the Instigator | — | tssequip.eqg | eqg-only (new item) |
| IT10905 | Drakkin Steel Greataxe / Regal Infused Giant Blade Axe / Regal Uninfused Giant Blade Axe | — | tssequip.eqg | eqg-only (new item) |
| IT10906 | Hook-Bladed Hand Axe | — | tssequip.eqg | eqg-only (new item) |
| IT10907 | Tainted Darksteel Battleaxe / Extravagant Infused Onyx Inlaid Hand Axe / Extravagant Uninfused Onyx Inlaid Hand Axe | — | tssequip.eqg | eqg-only (new item) |
| IT10908 | Rockbleeder / Silvery War Axe / Faded Silvery War Axe | — | tssequip.eqg | eqg-only (new item) |
| IT10909 |  | — | tssequip.eqg | eqg-only (new item) |
| IT10910 | Ache / Gold Plated Koshigatana | — | tssequip.eqg | eqg-only (new item) |
| IT10911 | Itch / Gilded Belt Knife / Venomous Stonehive Stinger | — | tssequip.eqg | eqg-only (new item) |
| IT10912 | Dryad Saber / Gold-Edged Saber / Paragon's Rapier | — | tssequip.eqg | eqg-only (new item) |
| IT10913 | Dryad Dagger / Knight Captain's Dagger / Bile Etched Ritual Dagger | — | tssequip.eqg | eqg-only (new item) |
| IT10914 | Icedancer / Potameid Saber / Sabre of Icy Retribution | — | tssequip.eqg | eqg-only (new item) |
| IT10915 | Icemaul's Fang / Potameid Dagger / Summoned: Winterbane | — | tssequip.eqg | eqg-only (new item) |
| IT10916 | Napaea Saber / Carved Bone Sabre / Tsetsian Plague Saber | — | tssequip.eqg | eqg-only (new item) |
| IT10917 | Napaea Dagger / Aurelia's Saber / Thornscrape Barb | — | tssequip.eqg | eqg-only (new item) |
| IT10918 | Oread Saber / Spellward's Saber | — | tssequip.eqg | eqg-only (new item) |
| IT10919 | Oread Dagger / Craita's Saber / Guardian Blade | — | tssequip.eqg | eqg-only (new item) |
| IT10920 | Bloodfoot Chopper / Gurrak Greatsword | — | tssequip.eqg | eqg-only (new item) |
| IT10921 | Malarian Plaguebringer | — | tssequip.eqg | eqg-only (new item) |
| IT10922 | Axe of the Brute / Zealot's Cleaver / Soldier's Striker | — | tssequip.eqg | eqg-only (new item) |
| IT10923 |  | — | tssequip.eqg | eqg-only (new item) |
| IT10924 | Soulcrush / Lord Tephys' Warhammer | — | tssequip.eqg | eqg-only (new item) |
| IT10925 | Spiked Brimstone Warhammer / Intricate Defiant Spiked Warhammer / Intricate Consigned Spiked Warhammer | — | tssequip.eqg | eqg-only (new item) |
| IT10926 | Kranigan's Fiery Hammer / Fleshmelter, Lethar's Maul / Spiked Hammer of Animated Lava | — | tssequip.eqg | eqg-only (new item) |
| IT10927 | Kaigrun's Maul of Malice | — | tssequip.eqg | eqg-only (new item) |
| IT10928 | Brightflame, Pride of the Lifebringer | — | tssequip.eqg | eqg-only (new item) |
| IT10929 | Spiked War Maul | — | tssequip.eqg | eqg-only (new item) |
| IT10930 | Elegant Defiant Square Hammer / Elegant Consigned Square Hammer | — | tssequip.eqg | eqg-only (new item) |
| IT10931 | Holmguard Maul / Broken Krithgor Maul / Elegant Defiant Warhammer | — | tssequip.eqg | eqg-only (new item) |
| IT10932 | Laaka, the Gnome Slayer / Brutal Gnome Sledgehammer / Runed Cold Iron Warhammer | — | tssequip.eqg | eqg-only (new item) |
| IT10933 | Fjorlask, War Maul of Shadows / Damaged Wraithguard Maul of Symbols | — | tssequip.eqg | eqg-only (new item) |
| IT10934 | Blunt Force / Prize: Blunt Force / Crystallized Quarreler | — | tssequip.eqg | eqg-only (new item) |
| IT10935 | Broken Maul of Zek / Warhammer of Divine Grace / Faded Warhammer of Divine Grace | — | tssequip.eqg | eqg-only (new item) |
| IT10936 | Shadow Forged Hammer / Frost Runed Icy War Hammer | — | tssequip.eqg | eqg-only (new item) |
| IT10937 | Hammer of the Apostate | — | tssequip.eqg | eqg-only (new item) |
| IT10938 | Simple Wooden Staff | — | tssequip.eqg | eqg-only (new item) |
| IT10939 | Bixie Dust Wand / Polymorph Wand: Izon / Stave of Stinging Soot | — | tssequip.eqg | eqg-only (new item) |
| IT10940 | Treantwood Staff | — | tssequip.eqg | eqg-only (new item) |
| IT10941 | Fulkitcher Weave / Wishka's Favor Trinket / Summoned: Windwoven Dreamcatcher | — | tssequip.eqg | eqg-only (new item) |
| IT10942 | Staff of Viral Flux / Staff of Final Rites | — | tssequip.eqg | eqg-only (new item) |
| IT10943 | Burial Mark / Inured Reinforced Baton / Ornate Reinforced Baton | — | tssequip.eqg | eqg-only (new item) |
| IT10944 | Fatespinner | — | tssequip.eqg | eqg-only (new item) |
| IT10945 | Dried Tree Limb / Broken Tree Club / Blessed Shillelagh | — | tssequip.eqg | eqg-only (new item) |
| IT10946 | Stone Kanabo / Lelluran's Mace / Hero's Spikemaul | — | tssequip.eqg | eqg-only (new item) |
| IT10947 | Castrum Heavy Spiked Club | — | tssequip.eqg | eqg-only (new item) |
| IT10948 | Dromrek Billy Club / Deepscar Painsmasher | — | tssequip.eqg | eqg-only (new item) |
| IT10949 | Smash / Minohten Hero Maul / Emperor Crush's Mattock | — | tssequip.eqg | eqg-only (new item) |
| IT10950 | War Forged Spear / Fieldstrider Spear | — | tssequip.eqg | eqg-only (new item) |
| IT10951 | Sanguine Spear / Nightmare Planar Spear / Barbarian Hunting Spear | — | tssequip.eqg | eqg-only (new item) |
| IT10952 | Satyr Spear / Blackfoot Fang / Ogre Watchtower Spear | — | tssequip.eqg | eqg-only (new item) |
| IT10953 | Indigo-Wrapped Spear / Fieldstrider Ceremonial Spear / Extravagant Infused Spiritist Spear | — | tssequip.eqg | eqg-only (new item) |
| IT10954 | Inured Short Spear / Ornate Short Spear / Simple Short Spear | — | tssequip.eqg | eqg-only (new item) |
| IT10955 | Bloodspear / Inured Long Spear / Ornate Long Spear | — | tssequip.eqg | eqg-only (new item) |
| IT10956 | Fieldstrider Spear / Splintered Grovewood Spear | — | tssequip.eqg | eqg-only (new item) |
| IT10957 | Deathspike Spear / Castrum Long Spear / Shadowpike of Toskirakk | — | tssequip.eqg | eqg-only (new item) |
| IT10958 | Void Shard / Elemental Shard / Worlu's Windcloak | — | tssequip.eqg | eqg-only (new item) |
| IT10959 | Floe / Ice Crystal Staff / Dancing Shard of Ice | — | tssequip.eqg | eqg-only (new item) |
| IT10960 | Iceglazed Shield / Battered Kromtus Shield / Steamforged Shield of Runes | — | tssequip.eqg | eqg-only (new item) |
| IT10961 | War Forged Great Shield / Ancient Inscribed Shield / Riven Krithgor Battle Shield | — | tssequip.eqg | eqg-only (new item) |
| IT10962 | Ruined Wraithguard Small Shield of Symbols | — | tssequip.eqg | eqg-only (new item) |
| IT10963 | Glyphed Barrier / Battered Silver Runed War Shield / Battered Shield of the Fallen Guard | — | tssequip.eqg | eqg-only (new item) |
| IT10964 | Frostguard | — | tssequip.eqg | eqg-only (new item) |
| IT10965 | Cold Iron Shield / Highkeeper's Haven | — | tssequip.eqg | eqg-only (new item) |
| IT10966 |  | — | tssequip.eqg | eqg-only (new item) |
| IT10967 |  | — | tssequip.eqg | eqg-only (new item) |
| IT10968 |  | — | tssequip.eqg | eqg-only (new item) |
| IT10969 | Alaran Arcanist Shield / Qeynos Arcanist Shield / Qeynos Conjuror Shield | — | tssequip.eqg | eqg-only (new item) |
| IT10970 | Cinderskin Buckler / Pyroplasmic Protector / Shield of the Flame Spirits | — | tssequip.eqg | eqg-only (new item) |
| IT10971 | Lava-Marked Obsidian Shield | — | tssequip.eqg | eqg-only (new item) |
| IT10972 |  | — | tssequip.eqg | eqg-only (new item) |
| IT10973 | Shield of Fire and Fury | — | tssequip.eqg | eqg-only (new item) |
| IT10974 |  | — | tssequip.eqg | eqg-only (new item) |
| IT10975 |  | — | tssequip.eqg | eqg-only (new item) |
| IT10976 | Chokidai Back Plate / Mordid's Back Plate | — | tssequip.eqg | eqg-only (new item) |
| IT10977 | Kaneida Hide Buckler / Tower Guardian's Shield / Fell Dragon Scale Shield | — | tssequip.eqg | eqg-only (new item) |
| IT10978 | Isinente's Shield | — | tssequip.eqg | eqg-only (new item) |
| IT10979 | Drakeskin Buckler / Dragonskin Buckler / Shield of the River Monster | — | tssequip.eqg | eqg-only (new item) |
| IT10980 | Warskin Shield / Ghoulskin Shield / King's Hide Shield | — | tssequip.eqg | eqg-only (new item) |
| IT10981 |  | — | tssequip.eqg | eqg-only (new item) |
| IT10982 | Battlemage's Aegis | — | tssequip.eqg | eqg-only (new item) |
| IT10983 | War Forged Blood-Shield / Axeweaver's Large Shield | — | tssequip.eqg | eqg-only (new item) |
| IT10984 | Battered Gnoll Shield / Torklar`s Battered Shield | — | tssequip.eqg | eqg-only (new item) |
| IT10985 | Large Satyr Shield | — | tssequip.eqg | eqg-only (new item) |
| IT10986 | Satyr Shield | — | tssequip.eqg | eqg-only (new item) |
| IT10987 | Gurrak Shield | — | tssequip.eqg | eqg-only (new item) |
| IT10988 | Grinning Skull Buckler | — | tssequip.eqg | eqg-only (new item) |
| IT10989 | Tower Champion's Shield / Shield of the Dark Waters | — | tssequip.eqg | eqg-only (new item) |
| IT10990 | Barrier of Black / Blackburrow Bulwark / Buckler of Mistmoore | — | tssequip.eqg | eqg-only (new item) |
| IT10991 | Fieldstrider Shield | — | tssequip.eqg | eqg-only (new item) |
| IT10992 | Traveller's Wardshield / Fieldstrider Legate Shield / Fieldstrider Captain Shield | — | tssequip.eqg | eqg-only (new item) |
| IT10993 | Swarm Shield / Web of Flies | — | tssequip.eqg | eqg-only (new item) |
| IT10994 | Fireborne Carapace | — | tssequip.eqg | eqg-only (new item) |
| IT10995 | Nexiont Shell Shield | — | tssequip.eqg | eqg-only (new item) |
| IT10996 | Discord Rider Shield | — | tssequip.eqg | eqg-only (new item) |
| IT10997 | Balath's Bow / Darkfell Bow / Bok Knight's Bow | — | tssequip.eqg | eqg-only (new item) |
| IT10998 | Soulstriker / Elkhorn Longbow / Castrum Long Bow | — | tssequip.eqg | eqg-only (new item) |
| IT10999 | Summoned: Searing Torch / Magmaraug's Burning Essence / Everflame Torch of Ashengate | — | tssequip.eqg | eqg-only (new item) |
| IT11001 | Piece of a Relic / Bokh's Dented Shield / Koby's Dented Shield | gequip3.s3d | — | modern .s3d only |
| IT11002 | Aqua Shield / Fortress Guard / Bloodling Shield | gequip3.s3d | — | modern .s3d only |
| IT11003 | Runed Dragon Scale / Patterned Dragon Scale / War Forged Wood Shield | gequip3.s3d | — | modern .s3d only |
| IT11013 | Tae Ew Shield / Sacred Grimling Shield / Blessed Grimling Shield | gequip4.s3d | — | modern .s3d only |
| IT11017 | Crow Wing / Nymph Wing / Gnomium Sample | gequip3.s3d, gequip4.s3d | — | REPLACED (later .s3d) |
| IT11018 | Plated Shield / Stout Bulwark / Dark Shield of the Scholar | gequip4.s3d | — | modern .s3d only |
| IT11019 | Validus Custodus Shield / Loyalist Shield of Honor / Gorum's Ashen Phoenix Aegis | gequip4.s3d | — | modern .s3d only |
| IT11020 | Scutum Veritas / Shield of Holy Vigor | gequip4.s3d | — | modern .s3d only |
| IT11031 | Burning Brand of War / Flame-tempered Brand / Summoned: Blazing Brand | — | tssequip.eqg | eqg-only (new item) |
| IT11032 |  | — | tssequip.eqg | eqg-only (new item) |
| IT11033 | Farmer's Hoe / Flawed Soldier's Hoe / Flawed Combatant's Hoe | — | tssequip.eqg | eqg-only (new item) |
| IT11034 | Farmer's Rake / Gardener's Rake / Flawed Adept's Rake | — | tssequip.eqg | eqg-only (new item) |
| IT11035 | Farmer's Pitchfork / Rongol's Pitchfork / Pitchfork of the Town Rebels | — | tssequip.eqg | eqg-only (new item) |
| IT11036 | Simple Bucket / Bucket of Waste / Full Water Bucket | — | tssequip.eqg | eqg-only (new item) |
| IT11037 |  | — | tssequip.eqg | eqg-only (new item) |
| IT11038 | Empty Tortoise Shell Bucket / Filled Tortoise Shell Bucket | — | tssequip.eqg | eqg-only (new item) |
| IT11039 | Trick or Treating Bucket / Zatozia's Pail of Rainbow Sherbet | — | tssequip.eqg | eqg-only (new item) |
| IT11040 | Pail of Slop / Pail of Water / Bucket of Slop | — | tssequip.eqg | eqg-only (new item) |
| IT11041 |  | — | tssequip.eqg | eqg-only (new item) |
| IT11042 | Dreamraze / Pestilence / Terror Steamblade | — | tssequip.eqg | eqg-only (new item) |
| IT11043 | Intricately Carved Greatsword | — | tssequip.eqg | eqg-only (new item) |
| IT11044 | Highmaul of Anguish / Crusher of the Crimson Dawn | — | tssequip.eqg | eqg-only (new item) |
| IT11045 | Greatmaul of the Prismatic Flight | — | tssequip.eqg | eqg-only (new item) |
| IT11046 | Stra'Hazir the Great Destroyer | — | tssequip.eqg | eqg-only (new item) |
| IT11047 |  | — | tssequip.eqg | eqg-only (new item) |
| IT11048 | Dreadiron Shield / Rathiss' Mudunugu Shield | — | tssequip.eqg | eqg-only (new item) |
| IT11049 | Targe of Kuua / Varuun's Battlehardened Targe | — | tssequip.eqg | eqg-only (new item) |
| IT11050 | Iceball / Snowball / Cloudball | — | tssequip.eqg | eqg-only (new item) |
| IT11051 | Froststrike / Snowblind Globe / Polymorph Wand: Polar Bear | — | tssequip.eqg | eqg-only (new item) |
| IT11052 | Dark Redeemer's Bludgeon / Sceptre of Draconic Calling / Heor`Otor the Prismatic Calamity | — | tssequip.eqg | eqg-only (new item) |
| IT11053 | Holagato's Decorative Blade / Faraday Stormrider's Fiery Avenger | — | tssequip.eqg | eqg-only (new item) |
| IT11054 | Curious Box / Stored Food / Anlut's Hint | — | tssequip.eqg | eqg-only (new item) |
| IT11055 | Bale of Hay / Spiced Barley | — | tssequip.eqg | eqg-only (new item) |
| IT11056 | Nettlevine / Docilla Leaf / Water Shoots | — | tssequip.eqg | eqg-only (new item) |
| IT11057 | Gor / Gnawstone / Slagstone | — | tssequip.eqg | eqg-only (new item) |
| IT11058 | Bone Chips / Oashim Meat / Bleached Bones | — | tssequip.eqg | eqg-only (new item) |
| IT11059 | Vampire Diary / Touch of the Six / Denizens of the Planes | — | tssequip.eqg | eqg-only (new item) |
| IT11060 | Kapok Leaves / Evergreen Leaf / Small Manisi Plant | — | tssequip.eqg | eqg-only (new item) |
| IT11061 | Brell's Bounty / Trampled Fungus / Heavenly Toadstool | — | tssequip.eqg | eqg-only (new item) |
| IT11062 | Gift-wrapped Toy / Wandering Thought / Kessrala's Package | — | tssequip.eqg | eqg-only (new item) |
| IT11063 | The Old Man's Lunch / Festively Packaged Toy / Heart Crystal Condenser | — | tssequip.eqg | eqg-only (new item) |
| IT11064 | Ether Ash / Sessiloid Dung / Jenni's Memento | — | tssequip.eqg | eqg-only (new item) |
| IT11065 | Drakkin Scale / Wanda's Charred Arm Bones | — | tssequip.eqg | eqg-only (new item) |
| IT11066 | Ragweed / Yew Leaf / Mash Tuber | — | tssequip.eqg | eqg-only (new item) |
| IT11067 | Dinobrush / Aquatic Plants / Jungle Water Root | — | tssequip.eqg | eqg-only (new item) |
| IT11068 | Bomb / Charcoal / Grimroot | — | tssequip.eqg | eqg-only (new item) |
| IT11069 | Tomato / Bloodbeet / Burning Ember | — | tssequip.eqg | eqg-only (new item) |
| IT11070 | Green Apple / Windberries / Kithicor Apple | — | tssequip.eqg | eqg-only (new item) |
| IT11071 | Jack Bomb / Ripe Pumpkin / Fancy Pumpkin | — | tssequip.eqg | eqg-only (new item) |
| IT11072 | Soulstone Shard / Tiny Parrot's Keel / Tiny Parrot's Skull | — | tssequip.eqg | eqg-only (new item) |
| IT11073 | Council Minutes / Zombies and You / Veeshan's Children | — | tssequip.eqg | eqg-only (new item) |
| IT11074 | Wiry Pondweed / Windwillow Branch / Ancient Elddar Branch | — | tssequip.eqg | eqg-only (new item) |
| IT11075 | Giant Leaf / Collected Water / Lost Lord's Promise | — | tssequip.eqg | eqg-only (new item) |
| IT11076 | Saltweed / Bamboo Shoot / Glowing Ember | — | tssequip.eqg | eqg-only (new item) |
| IT11077 | Willowcrush's Branch / Straight Oak Treant Branch / Fallen Grovewalker Treant Branch | — | tssequip.eqg | eqg-only (new item) |
| IT11078 | Assorted Wood / Bitter Lava Root / Dry Treant Branch | — | tssequip.eqg | eqg-only (new item) |
| IT11079 | Broadleaf / Twisted Twig Charm | — | tssequip.eqg | eqg-only (new item) |
| IT11080 | Flawless Autumn Leaf | — | tssequip.eqg | eqg-only (new item) |
| IT11081 | Deep Deathcap / Mushroom Idol / Toadstool Idol | — | tssequip.eqg | eqg-only (new item) |
| IT11082 | Mushroom Spores | — | tssequip.eqg | eqg-only (new item) |
| IT11083 | Cave Mushroom / Fancy Mushroom / Blackened Deathcap | — | tssequip.eqg | eqg-only (new item) |
| IT11084 | Fallen Branch / GS-Test Branch - Brown - Leaves | — | tssequip.eqg | eqg-only (new item) |
| IT11085 | Shield of a Thousand Curses / Muse Shield of Plank and Bones / Brute Shield of Plank and Bones | — | tbsequip.eqg | eqg-only (new item) |
| IT11086 | Iron Wood Bulwark / Waterlogged Wheel / Silius's Waterlogged Wheel | — | tbsequip.eqg | eqg-only (new item) |
| IT11087 | Statuae Clipium / Aviak Spirit Totem / Holgresh Metamorph Totem | — | tbsequip.eqg | eqg-only (new item) |
| IT11088 | Worthless Baling Hook | — | tbsequip.eqg | eqg-only (new item) |
| IT11089 | Corpse Hook / Dragorn-Rib Hook / Bertoxxulous' Crook | — | tbsequip.eqg | eqg-only (new item) |
| IT11090 | Iridescent Coral Saber / Sword of the Buried Deep | — | tbsequip.eqg | eqg-only (new item) |
| IT11091 | Indagatrix Blade / Carved Ivory Stiletto / Traitor's Cursed Shank | — | tbsequip.eqg | eqg-only (new item) |
| IT11092 | Trident of the Buried Deep / August Infused Spear of Beauty / August Uninfused Spear of Beauty | — | tbsequip.eqg | eqg-only (new item) |
| IT11093 | Fishmonger's Trident / Iridescent Coral Trident / Trident of the Dark Depths | — | tbsequip.eqg | eqg-only (new item) |
| IT11094 | Arc of Pain / Bloodreaper / Shark-tooth Ripper | — | tbsequip.eqg | eqg-only (new item) |
| IT11095 | Bejeweled Gemstone Knife / Jagged Iridescent Dagger / Jagged Shark-tooth Dagger | — | tbsequip.eqg | eqg-only (new item) |
| IT11096 | Denmother's Rolling Pin | — | tbsequip.eqg | eqg-only (new item) |
| IT11097 | Hissing Staff / Staff of Apotheosis | — | tbsequip.eqg | eqg-only (new item) |
| IT11098 | Rod of Apotheosis / Wire Wrapped Sceptre / Elegant Defiant Sceptre | — | tbsequip.eqg | eqg-only (new item) |
| IT11099 | Soulsunder / Paragon's Ulak / Deadly Weeder's Knives | — | tbsequip.eqg | eqg-only (new item) |
| IT11100 | Castrum Long Sword | — | tbsequip.eqg | eqg-only (new item) |
| IT11101 | Shockpiercer / Castrum Short Sword | — | tbsequip.eqg | eqg-only (new item) |
| IT11102 | Castrum Buckler / White Coral Buckler / Shimmering Galeforce Aegis | — | tbsequip.eqg | eqg-only (new item) |
| IT11103 | Shield of the Combine Empire | — | tbsequip.eqg | eqg-only (new item) |
| IT11104 | Vide Infra / Soulmason Crook / Energized Hydraulic Rod | — | tbsequip.eqg | eqg-only (new item) |
| IT11105 | Taskmaster's Flail / Flail of the Fickle Flayer | — | tbsequip.eqg | eqg-only (new item) |
| IT11106 | Incantators Hammer / Burning Mace of Judgement | — | tbsequip.eqg | eqg-only (new item) |
| IT11107 | Twisted Staff of Temporal Paradox | — | tbsequip.eqg | eqg-only (new item) |
| IT11108 | Sun Tempered Lunar Blade | — | tbsequip.eqg | eqg-only (new item) |
| IT11109 | Great Axe of Twin Flames / Power-Infused Steamcleaver / Great Axe of Twin Flames Ornamentation | — | tbsequip.eqg | eqg-only (new item) |
| IT11110 | Burning Seal of Truth / Buckler of Pleasant Dreams / Bulwark of the Firebringer | — | tbsequip.eqg | eqg-only (new item) |
| IT11111 | Rekkter the Red / Shield of Solteris / Pulsating Beacon of Fire | — | tbsequip.eqg | eqg-only (new item) |
| IT11112 | Sparking Short Sword of Magma | — | tbsequip.eqg | eqg-only (new item) |
| IT11113 | Blade of Pure Light / Dancing Blade of Fire / Prize: Dancing Blade of Fire | — | tbsequip.eqg | eqg-only (new item) |
| IT11114 | Dagger of Insanity / Dagger of Darkflaying / Venomous Fang of the Seer | — | tbsequip.eqg | eqg-only (new item) |
| IT11115 | Darkearth Scimitar / Scimitar of The Damned / Twisting Whirlwind Blade | — | tbsequip.eqg | eqg-only (new item) |
| IT11116 | Pike of the Vampire God | — | tbsequip.eqg | eqg-only (new item) |
| IT11117 | Orb of Striking / Mounted Prismatic Focus-Gem | — | tbsequip.eqg | eqg-only (new item) |
| IT11118 | Gnomish Troutshocker / Imperfect Gnomish Troutshocker / Gyrospire Zeka Gyroscope 268'119'204' | — | tbsequip.eqg | eqg-only (new item) |
| IT11119 | Lance of Solteris | — | tbsequip.eqg | eqg-only (new item) |
| IT11120 | Fist of Lava / Solusek's Eye / Ball of Sunlight | — | tbsequip.eqg | eqg-only (new item) |
| IT11121 | Money / Doubloon / Bayle Mark | — | tbsequip.eqg | eqg-only (new item) |
| IT11122 | Orum / Silver Cymbals / Assassin's Coin | — | tbsequip.eqg | eqg-only (new item) |
| IT11123 | Phosphite / Midnight Sunshard / Elaborate Soldier's Crystal | — | tbsequip.eqg | eqg-only (new item) |
| IT11124 | Phosphene / Phosphene Cluster / Tiny Phosphene Cluster | — | tbsequip.eqg | eqg-only (new item) |
| IT11128 | Ageless Lantern | — | sofequip.eqg | eqg-only (new item) |
| IT11129 |  | — | sofequip.eqg | eqg-only (new item) |
| IT11130 |  | — | sofequip.eqg | eqg-only (new item) |
| IT11131 |  | — | sofequip.eqg | eqg-only (new item) |
| IT11132 | Flaming Sword of War / Cauterizing Greatsword of Agony | — | sofequip.eqg | eqg-only (new item) |
| IT11133 |  | — | sofequip.eqg | eqg-only (new item) |
| IT11134 |  | — | sofequip.eqg | eqg-only (new item) |
| IT11135 |  | — | sofequip.eqg | eqg-only (new item) |
| IT11138 | Stalking Probe / Gnomish Handcannon / Gnomish Steamfists | — | sofequip.eqg | eqg-only (new item) |
| IT11139 | Stones of Fortune | — | sofequip.eqg | eqg-only (new item) |
| IT11140 | Armored Orcakar Figurine | — | sofequip.eqg | eqg-only (new item) |
| IT11141 | Worn Blue doll | — | sofequip.eqg | eqg-only (new item) |
| IT11142 | Decorin Buckler / Targe of Kulthar / Gilded Targe of Kulthar | — | sofequip.eqg | eqg-only (new item) |
| IT11143 | Crawtooth's Shield / Qeynos Evoker Shield / Bloodmoon Warrior Crest | — | sofequip.eqg | eqg-only (new item) |
| IT11144 | Aethercast Shield of Redemption / Aethercast Shield of Greater Redemption | — | sofequip.eqg | eqg-only (new item) |
| IT11145 | Third Eye Shield | — | sofequip.eqg | eqg-only (new item) |
| IT11146 | Spernal Fetish / Xixkt's Fetish / Attendant's Fetish | — | sofequip.eqg | eqg-only (new item) |
| IT11147 | Mossy Cudgel / Club of Spores / Fungi Coated Hammer | — | sofequip.eqg | eqg-only (new item) |
| IT11148 | Orrel's War-Staff | — | sofequip.eqg | eqg-only (new item) |
| IT11149 | Shadowy Blackblade / Blade of the Mature Wyvern / Blazon, Bane of the Wyvern | — | sofequip.eqg | eqg-only (new item) |
| IT11150 | Gleaming Minotaur Waraxe / Mantoc, the Horn Cleaver / Ruthless Minotaur Waraxe | — | sofequip.eqg | eqg-only (new item) |
| IT11152 | Lunar Horns / The Foul Wind / Kyv Chitin Spikes | — | sofequip.eqg | eqg-only (new item) |
| IT11153 | Hero's Minotaur Staff / Staff of the Patriarch / Elaborate Defiant Minotaur Staff | — | sofequip.eqg | eqg-only (new item) |
| IT11154 | Amethyst Store / Spent Dreambolt / Rough Purple Gem | — | sofequip.eqg | eqg-only (new item) |
| IT11155 | Rough Green Gem / Median Essence of Decay / Crystallized Growth Store | — | sofequip.eqg | eqg-only (new item) |
| IT11156 | Rough Red Gem / Lesser Terrormote / Minor Essence of Decay | — | sofequip.eqg | eqg-only (new item) |
| IT11157 | Rough Blue Gem / Sapphire Store / Median Terrormote | — | sofequip.eqg | eqg-only (new item) |
| IT11158 | Rough Yellow Gem / Crystallized Refuse / Lesser Essence of Decay | — | sofequip.eqg | eqg-only (new item) |
| IT11159 | Shard of Storms / Glowing Terrormote / Necrocrystal Fragment | — | sofequip.eqg | eqg-only (new item) |
| IT11160 | Gem of Power / Clear Crystal / Cloudy Crystal | — | sofequip.eqg | eqg-only (new item) |
| IT11161 | Staff of Angry Dreams / Crystal of the Tenth Celebration | — | sofequip.eqg | eqg-only (new item) |
| IT11162 | Barrier Shard / Dim Fearshard / Amethyst Cache | — | sofequip.eqg | eqg-only (new item) |
| IT11163 | Faded Fearshard / Druid Range (Test) / Bile Drenched Jewel | — | sofequip.eqg | eqg-only (new item) |
| IT11164 | Steamshard / Balefire Brand / Dull Fearshard | — | sofequip.eqg | eqg-only (new item) |
| IT11165 | Stone Tear / Sibuncu Horn / Sylphid Shard | — | sofequip.eqg | eqg-only (new item) |
| IT11166 | Opalescent Seascale / Lesser Briny Essence / Lesser Essence of Life | — | sofequip.eqg | eqg-only (new item) |
| IT11167 | Glowing Dreadmote / Sliver of Darkness / Scryer Ytta's Shard | — | sofequip.eqg | eqg-only (new item) |
| IT11168 | Coral Shard / Frozen Totem / Luminous Stone | — | sofequip.eqg | eqg-only (new item) |
| IT11169 | Soulcapture Crystal / Curio of Scintillation | — | sofequip.eqg | eqg-only (new item) |
| IT11170 | Steamwork Welder's Multitool | — | sofequip.eqg | eqg-only (new item) |
| IT11171 | Stub-nosed Pliers / Tinkered Catapult / Metamorph Totem: Steamwork Fixer | — | sofequip.eqg | eqg-only (new item) |
| IT11172 | All-Purpose Longwrench / Tristos' Weighted Wrench / Large Jaws of the Mechanic | — | sofequip.eqg | eqg-only (new item) |
| IT11173 | Jaws of the Mechanic / Steelgrip Power-Wrench / Polymorph Wand: Twisted Gnomework | — | sofequip.eqg | eqg-only (new item) |
| IT11174 | Which Came First / Kidney-Removing Autoshank / Mechanomagical Grinder Drill | — | sofequip.eqg | eqg-only (new item) |
| IT11175 | Steam-Honed Impaler / Trainer's Drill Arm / Industrial Tinkered Drillbore | — | sofequip.eqg | eqg-only (new item) |
| IT11176 | Experimental Clockwork Blade | — | sofequip.eqg | eqg-only (new item) |
| IT11177 | Hydraulic Crushblade | — | sofequip.eqg | eqg-only (new item) |
| IT11178 | Fontpine Cutting Device / Massive Steam-Powered Axe / Prized Gear of the Autarch Tinkerers | — | sofequip.eqg | eqg-only (new item) |
| IT11179 | Pneumatic Powerblade | — | sofequip.eqg | eqg-only (new item) |
| IT11180 | Mechlo Sprockets / Studded Engineer's Workgrips / Elegant Defiant Knuckle Guards | — | sofequip.eqg | eqg-only (new item) |
| IT11181 | Troopleader's Sword / Neppigrim's Longcutter / Chain-Powered Longcutter | — | sofequip.eqg | eqg-only (new item) |
| IT11182 | Pointed Control Shaft / Jagged Alloy Repeato-Shiv / Naglebrak's Cleansing Tool | — | sofequip.eqg | eqg-only (new item) |
| IT11183 | Buckler of Blood / Shield of Dancing Flame / Ichor-Splattered Buckler | — | sofequip.eqg | eqg-only (new item) |
| IT11184 |  | — | sofequip.eqg | eqg-only (new item) |
| IT11185 | Survivor's Guildshield | — | sofequip.eqg | eqg-only (new item) |
| IT11186 |  | — | sofequip.eqg | eqg-only (new item) |
| IT11187 |  | — | sofequip.eqg | eqg-only (new item) |
| IT11188 | Slimy Dragon Emblazoned Shield | — | sofequip.eqg | eqg-only (new item) |
| IT11189 | Tower Shield of Erendos / Shield of the Erendosman | — | sofequip.eqg | eqg-only (new item) |
| IT11190 | Frosted Buckler | — | sofequip.eqg | eqg-only (new item) |
| IT11191 | Fantastical Guardian Shield | — | sofequip.eqg | eqg-only (new item) |
| IT11192 |  | — | sofequip.eqg | eqg-only (new item) |
| IT11193 | Likato Fistwraps / Swirling Fist of Fog / Shimmering Lightstone | — | sofequip.eqg | eqg-only (new item) |
| IT11194 | Maul of the Elder Wyvern | — | sofequip.eqg | eqg-only (new item) |
| IT11195 | Wrext Mal Mesmerist's Focus | — | sofequip.eqg | eqg-only (new item) |
| IT11196 | Tempestuous Longsword / Heuthlor, Bane of the Wyvern / Longblade of the Mature Wyvern | — | sofequip.eqg | eqg-only (new item) |
| IT11197 | Shank of the Eternal Squall | — | sofequip.eqg | eqg-only (new item) |
| IT11198 | Glacier-Crystal Lance / High Lance of Draconic Mastery / Prismatic Greatspear of Bloodshed | — | sofequip.eqg | eqg-only (new item) |
| IT11199 | Lance of the Ward | — | sofequip.eqg | eqg-only (new item) |
| IT11200 | Timeshear / Chronobine / Dream Mote | — | sofequip.eqg | eqg-only (new item) |
| IT11201 | Snowy Daffodil / Blossom of Logos / White Flower of Functionality | — | sofequip.eqg | eqg-only (new item) |
| IT11202 |  | — | sofequip.eqg | eqg-only (new item) |
| IT11203 | Frightbloom / Fresh Purple Flower / Wilted Purple Flower | — | sofequip.eqg | eqg-only (new item) |
| IT11204 | Water Arum / Rosi Auriun / Stem of Destruction | — | sofequip.eqg | eqg-only (new item) |
| IT11205 | Corn Rose / Ro's Rose / Ruby Aster | — | sofequip.eqg | eqg-only (new item) |
| IT11206 | Dried Grave-Flower / Green Flower of Functionality | — | sofequip.eqg | eqg-only (new item) |
| IT11207 | Stone Vial / Purest Water / Tabron's Remedy | — | sofequip.eqg | eqg-only (new item) |
| IT11208 | Memory Tonic / Empty Water Urn / Fractured Pottery | — | sofequip.eqg | eqg-only (new item) |
| IT11209 | Lucky Coin Pouch / Watch Tower Stash / Distraction Device | — | sofequip.eqg | eqg-only (new item) |
| IT11210 | Twisted Fire Wand / Wand of Pelagic Modulation / Lightweaver's Enchanting Wand | — | sofequip.eqg | eqg-only (new item) |
| IT11211 | Ironwood Wand / The Lady's Entreaty / Wand of Pelagic Transvergence | — | sofequip.eqg | eqg-only (new item) |
| IT11212 | Mundunugu Medicine Stick / Token of the Conjoined Spirit | — | sofequip.eqg | eqg-only (new item) |
| IT11214 | Mug / Garuf's Ale / Somnolent Beer | — | sofequip.eqg | eqg-only (new item) |
| IT11215 | Fletcher's Arrow | — | sofequip.eqg | eqg-only (new item) |
| IT11216 | Family Heirloom / Intricate Jewelers Glass | — | sofequip.eqg | eqg-only (new item) |
| IT11217 | Peerless Pestle | — | sofequip.eqg | eqg-only (new item) |
| IT11218 | Sludge Flinger / Clay Flinger's Loop | — | sofequip.eqg | eqg-only (new item) |
| IT11219 | Blacksmith's Adamantine Hammer | — | sofequip.eqg | eqg-only (new item) |
| IT11220 | Ethereal Quill / Nostulia's Quill / Sharp Ayrlak Tooth | — | sofequip.eqg | eqg-only (new item) |
| IT11221 | Mystical Bolt | — | sofequip.eqg | eqg-only (new item) |
| IT11222 | Battle Orb / Hovering Contraption / Mizzenblast Exoblast | — | sofequip.eqg | eqg-only (new item) |
| IT11223 | Trophy of Filth / Trophy of Sleep / Trophy of Danela | — | sofequip.eqg | eqg-only (new item) |
| IT11224 | Savage Shield / Shield of the Domain / Kite Shield of Arcane Spellshock | — | sofequip.eqg | eqg-only (new item) |
| IT11252 | Spinal Extractor / Intruderbane Hook / Brodha's Gaff Hook | — | sofequip.eqg | eqg-only (new item) |
| IT11253 | Ghostly Lantern / Wisplit Lantern / Black Powder Bomb | — | sofequip.eqg | eqg-only (new item) |
| IT11254 | Beacon of Lost Souls / Second Sight Lantern / Resplendent Guiding Light | — | sofequip.eqg | eqg-only (new item) |
| IT11255 | Brell Statuettes / Carving of Oseka / Chipped Statuettes | — | sofequip.eqg | eqg-only (new item) |
| IT11257 | Discus of Ord / Red Boomerang / Blue Boomerang | — | sodequip.eqg | eqg-only (new item) |
| IT11258 | Pile of Rot / Orb of the Deep / Essence of Suffering | — | sofequip.eqg | eqg-only (new item) |
| IT11259 | Ghastly Orb / Firegem of Thex / Vision of Tomorrow | — | sofequip.eqg | eqg-only (new item) |
| IT11260 | Shiny Thing / Glimflash Gem / Orb of the Sky | — | sofequip.eqg | eqg-only (new item) |
| IT11261 | Earthen Bile / Rainfall Emblem / Dark Storm Emblem | — | sofequip.eqg | eqg-only (new item) |
| IT11262 | Kraken's Eye / Combined Essences / Focus of Black Rage | — | sofequip.eqg | eqg-only (new item) |
| IT11263 | Ooze Sample / Tainted Lichen Sample | — | sofequip.eqg | eqg-only (new item) |
| IT11264 | Bazu Slime / Jungle Moss / Caveflower Pollen | — | sofequip.eqg | eqg-only (new item) |
| IT11265 | Regurgitated Lichen | — | sofequip.eqg | eqg-only (new item) |
| IT11266 | Inkblot / Lichenash / Rotting Mold | — | sofequip.eqg | eqg-only (new item) |
| IT11267 | Recording Crystal / Pile of Mithril Ore | — | sofequip.eqg | eqg-only (new item) |
| IT11268 |  | — | sofequip.eqg | eqg-only (new item) |
| IT11269 |  | — | sofequip.eqg | eqg-only (new item) |
| IT11270 | Harmonic Crystal / Inspiring Insight / Clear Ocean Quartz | — | sofequip.eqg | eqg-only (new item) |
| IT11271 |  | — | sofequip.eqg | eqg-only (new item) |
| IT11272 |  | — | sofequip.eqg | eqg-only (new item) |
| IT11273 |  | — | sofequip.eqg | eqg-only (new item) |
| IT11311 | Darkfall / Soulscourge / Rod of Dark Rituals | — | sodequip.eqg | eqg-only (new item) |
| IT11312 | Clayforger / Shellcrusher | — | sodequip.eqg | eqg-only (new item) |
| IT11313 | Monstrous Mace | — | sodequip.eqg | eqg-only (new item) |
| IT11314 | Ebonlight / Razorblade of Scale | — | sodequip.eqg | eqg-only (new item) |
| IT11315 | Fearspine / Golemstone Shard / Dragonhook Flay Axe | — | sodequip.eqg | eqg-only (new item) |
| IT11316 | Consersa's Reaver | — | sodequip.eqg | eqg-only (new item) |
| IT11317 | Tower Champion's Fork | — | sodequip.eqg | eqg-only (new item) |
| IT11318 | Marrowscribe | — | sodequip.eqg | eqg-only (new item) |
| IT11319 | Soulcleave / Royal Iksar Court Javelin | — | sodequip.eqg | eqg-only (new item) |
| IT11320 | Lizard Spine Coercer / Deathleaper's Tenderizer / Master Cosensal's Smasher | — | sodequip.eqg | eqg-only (new item) |
| IT11321 | Barrelbreaker / Muramite Black Maul / Spikemangler's Dulled Shoulderspike | — | sodequip.eqg | eqg-only (new item) |
| IT11322 | Xangef's Nightmace / Hexxt Shredder's Mace / Deathstalker's Spiked Bowbender | — | sodequip.eqg | eqg-only (new item) |
| IT11323 | Chainrender's Mace / Maul of Dire Intent / Flashstriker's Great Mace | — | sodequip.eqg | eqg-only (new item) |
| IT11324 | Rend / Klygg's Haladine / Blade of Far Sight | — | sodequip.eqg | eqg-only (new item) |
| IT11325 | Cutter / Mindrend / Shadowed Soulblade | — | sodequip.eqg | eqg-only (new item) |
| IT11326 | Chaxyl's Voulge / Triskixx the Sweeper | — | sodequip.eqg | eqg-only (new item) |
| IT11327 |  | — | sodequip.eqg | eqg-only (new item) |
| IT11328 | Lifeshriek | — | sodequip.eqg | eqg-only (new item) |
| IT11329 | Shadowhorn / Time's Sundering | — | sodequip.eqg | eqg-only (new item) |
| IT11330 | Dire Cestii / Flesh Scourge / Hexxt Chitinbreaker's Lessonteacher | — | sodequip.eqg | eqg-only (new item) |
| IT11341 | Sigil of the Gate / Fippy's Conquering Shield / Qeynos Shield of Foundation | — | sodequip.eqg | eqg-only (new item) |
| IT11375 |  | — | undequip.eqg | eqg-only (new item) |
| IT11376 |  | — | undequip.eqg | eqg-only (new item) |
| IT11377 |  | — | undequip.eqg | eqg-only (new item) |
| IT11402 | Enforcing Hatchet / Axe of the Sunderer | — | undequip.eqg | eqg-only (new item) |
| IT11403 | Spiderback Axe / Subjugator's Eversharp Axe | — | undequip.eqg | eqg-only (new item) |
| IT11404 | Anvil Forged Axe | — | undequip.eqg | eqg-only (new item) |
| IT11405 | Gyi's Maul / Dwarven Martel / Calcified Martel | — | undequip.eqg | eqg-only (new item) |
| IT11406 | Dwarven War Staff / High Priest War Staff / Rager's Beating Stick | — | undequip.eqg | eqg-only (new item) |
| IT11407 | Ancill's Hatchet / Tracker's Skullripper | — | undequip.eqg | eqg-only (new item) |
| IT11408 | Firestrike Spear / Brute's War Spear / Spear of Lost Time | — | undequip.eqg | eqg-only (new item) |
| IT11409 | Morose Puncher / Mephit Stabbing Katar | — | undequip.eqg | eqg-only (new item) |
| IT11410 | Ash-covered Shield / Telmiran War Shield / Shield of Gnollish Rage | — | undequip.eqg | eqg-only (new item) |
| IT11411 | Foehatchet / Beguiler's Last Answer / Fallen Leviathan's Runic Hatchet | — | undequip.eqg | eqg-only (new item) |
| IT11412 | Labrys of the Twisted | — | undequip.eqg | eqg-only (new item) |
| IT11413 | Unneksteel Greataxe | — | undequip.eqg | eqg-only (new item) |
| IT11414 | Trag's Digger / Envoy's Statement / Dissenting Pounder | — | undequip.eqg | eqg-only (new item) |
| IT11415 | Bonebraided Staff | — | undequip.eqg | eqg-only (new item) |
| IT11416 | Torment | — | undequip.eqg | eqg-only (new item) |
| IT11417 | Collection Probe | — | undequip.eqg | eqg-only (new item) |
| IT11418 |  | — | undequip.eqg | eqg-only (new item) |
| IT11419 |  | — | undequip.eqg | eqg-only (new item) |
| IT11420 | Edge of Oblivion / Sword of Brell's Ire / Sword of Brell's Fury | — | undequip.eqg | eqg-only (new item) |
| IT11421 | Immortal Sunstrike Mace / Renowned Sunstrike Mace / Brilliant Sunstrike Mace | — | undequip.eqg | eqg-only (new item) |
| IT11422 | Superb Lance of Neverending Light / Honored Lance of Neverending Light / Notable Lance of Neverending Light | — | undequip.eqg | eqg-only (new item) |
| IT11423 | Arctic Sword of Glacial Power / Chilly Sword of Glacial Power / Frigid Sword of Glacial Power | — | undequip.eqg | eqg-only (new item) |
| IT11424 | Mysterious Sword / Globe of Pure Light / Vile Sword of Screaming Souls | — | undequip.eqg | eqg-only (new item) |
| IT11425 | Tanglevine Scimitar of Holding / Tanglevine Scimitar of Grasping / Tanglevine Scimitar of Gripping | — | undequip.eqg | eqg-only (new item) |
| IT11426 | Fist of the Adept / Fist of the Master / Fist of the Novice | — | undequip.eqg | eqg-only (new item) |
| IT11427 | Phosphorescent Rapier of Dim Light / Phosphorescent Rapier of Faint Light / Phosphorescent Rapier of Bright Light | — | undequip.eqg | eqg-only (new item) |
| IT11428 | Thug's Bloodblade Dagger / Rogue's Bloodblade Dagger / Burglar's Bloodblade Dagger | — | undequip.eqg | eqg-only (new item) |
| IT11429 | Fate-Touched Spear of the Mystic / Fate-Touched Spear of the Oracle / Fate-Touched Spear of the Prophet | — | undequip.eqg | eqg-only (new item) |
| IT11430 | Wraith's Blade of Aphotic Rites / Defiler's Blade of Aphotic Rites / Heretic's Blade of Aphotic Rites | — | undequip.eqg | eqg-only (new item) |
| IT11431 | Incandescent Scepter of the Adept Arcanist / Incandescent Scepter of the Master Arcanist / Incandescent Scepter of the Eternal Arcanist | — | undequip.eqg | eqg-only (new item) |
| IT11432 | Faded Orb of the Magus / Glowing Orb of the Magus / Splendid Orb of the Magus | — | undequip.eqg | eqg-only (new item) |
| IT11433 | Enigma's Staff of Second Sight / Beguiler's Staff of Second Sight / Visionary's Staff of Second Sight | — | undequip.eqg | eqg-only (new item) |
| IT11434 | Brutal Ripslice Claws / Carnal Ripslice Claws / Bestial Ripslice Claws | — | undequip.eqg | eqg-only (new item) |
| IT11435 | Yngvar's Battleaxe of Harm / Yngvar's Battleaxe of Ruin / Yngvar's Battleaxe of Havoc | — | undequip.eqg | eqg-only (new item) |
| IT11436 | Chitin-Joint Bow / Arms of the Cliknar / Deathshattering Bow | — | undequip.eqg | eqg-only (new item) |
| IT11437 | Sporerune Bow / Stonebark Bow | — | undequip.eqg | eqg-only (new item) |
| IT11438 | Inured Recurve Bow / Isopexine Swiftbow / Ornate Recurve Bow | — | undequip.eqg | eqg-only (new item) |
| IT11439 | Bloodscream | — | undequip.eqg | eqg-only (new item) |
| IT11440 | Tower of Gold / Alaran Resolute Shield / Qeynos Resolute Shield | — | undequip.eqg | eqg-only (new item) |
| IT11441 |  | — | undequip.eqg | eqg-only (new item) |
| IT11442 | Mad Sizentist Shield / Deathspore Cap Shield | — | undequip.eqg | eqg-only (new item) |
| IT11443 | Brassbound Ward / Adorned Shield of Potency / Buckler of the Capricious Firebug | — | undequip.eqg | eqg-only (new item) |
| IT11444 | Hunter's Trap / Brax Nae Defender / Hive Custodian's Blade | — | undequip.eqg | eqg-only (new item) |
| IT11445 | Collyx's Cleaver | — | undequip.eqg | eqg-only (new item) |
| IT11446 | Tatraatix Sporecap / Hive Nurse's Corrector / Scepter of the War Priest | — | undequip.eqg | eqg-only (new item) |
| IT11447 | Merl's Problem Solver / Glowing Cliknar Antenna / Severed Guardian Antenna | — | undequip.eqg | eqg-only (new item) |
| IT11448 | Snapjaw Fang / Razorjaw Fang / Dactyl Fused Dagger | — | undequip.eqg | eqg-only (new item) |
| IT11449 | Cliknar Impaler / Spider Tender's Goad | — | undequip.eqg | eqg-only (new item) |
| IT11450 | Hunter's Claw / Claws of the Orc Lord | — | undequip.eqg | eqg-only (new item) |
| IT11451 | Insanity Fangs | — | undequip.eqg | eqg-only (new item) |
| IT11452 | Xax Shell Shield / Aegis of the Fallen / Indurate Chitin Buckler | — | undequip.eqg | eqg-only (new item) |
| IT11453 | Black Revenge / Flintbone Sword / Fire-formed Obsidian Shard | — | undequip.eqg | eqg-only (new item) |
| IT11454 |  | — | undequip.eqg | eqg-only (new item) |
| IT11455 | Ebon Crook / Ashenstrike / Choxar's Crook | — | undequip.eqg | eqg-only (new item) |
| IT11456 | Bloodied Crook of Wrath | — | undequip.eqg | eqg-only (new item) |
| IT11457 | Firefang | — | undequip.eqg | eqg-only (new item) |
| IT11458 | Stoneheart Spear / Regal Infused Huntsman's Spear / Regal Uninfused Huntsman's Spear | — | undequip.eqg | eqg-only (new item) |
| IT11459 | Knuckles of Pain / Lavaflow Stonefist / Claws of the Destroyer | — | undequip.eqg | eqg-only (new item) |
| IT11460 | Obsidian Slice / Blackscale Buckler | — | undequip.eqg | eqg-only (new item) |
| IT11461 | Etched Axe of Judgment / War-torn Axe of Mayhem / Axe of the Fungal Gloom | — | undequip.eqg | eqg-only (new item) |
| IT11462 | Earthrend / Winged Axe of Rebellion | — | undequip.eqg | eqg-only (new item) |
| IT11463 | Heart of the Underfoot War Axe / Knight's Heart of the Underfoot War Axe | — | undequip.eqg | eqg-only (new item) |
| IT11464 | Scepter of Creation / Growth Covered Martel / Hammer of Ravenous Lichen | — | undequip.eqg | eqg-only (new item) |
| IT11465 | Battle Staff of Torment / Sinister Staff of the Dim | — | undequip.eqg | eqg-only (new item) |
| IT11466 | Razor of the Underfoot / Kris of Failed Creation / Curved Blade of the Destructor | — | undequip.eqg | eqg-only (new item) |
| IT11467 | Mirelance of the Shadows | — | undequip.eqg | eqg-only (new item) |
| IT11468 | Katar of the Temple / Katar of Bound Elements / August Infused Knuckle Blade | — | undequip.eqg | eqg-only (new item) |
| IT11469 | Bladed Shield of Havoc / Battletested Shield of Poise | — | undequip.eqg | eqg-only (new item) |
| IT11470 | Runemuse Hatchet / Cassarah, Iron Will of Brell / Hatchet of the Final Assault | — | undequip.eqg | eqg-only (new item) |
| IT11471 | Forcible Axe of the Ward / Hephae, Restorer of First's Laurels | — | undequip.eqg | eqg-only (new item) |
| IT11472 | Heart of the First War Axe / Knight's Heart of the First War Axe | — | undequip.eqg | eqg-only (new item) |
| IT11473 | Darkheart the Unstable / Magi's Mallet of Purest Energy / Whisperwind, Kertrasia's Voice | — | undequip.eqg | eqg-only (new item) |
| IT11474 | Staff of Earthen Retribution / Acrimony, Morthira's Corruption | — | undequip.eqg | eqg-only (new item) |
| IT11475 | Thel Dagger / Flowing Clay Carver / Creation's Destruction | — | undequip.eqg | eqg-only (new item) |
| IT11476 | Grall's Lost Spear / Firstborne, Brell's Equal | — | undequip.eqg | eqg-only (new item) |
| IT11477 | Bloodletter, Katar of the Sisters | — | undequip.eqg | eqg-only (new item) |
| IT11478 | Bulwark of Stability / Shining Guard of the Underfoot | — | undequip.eqg | eqg-only (new item) |
| IT11479 | Blade of Living Clay | — | undequip.eqg | eqg-only (new item) |
| IT11480 | Distortion Greatsword / Lake-Cooled Greatsword / Blessed Sword of Strategic Combat | — | undequip.eqg | eqg-only (new item) |
| IT11481 | Pumice Mace / Doll Smasher / Ruinous Baton | — | undequip.eqg | eqg-only (new item) |
| IT11482 | Cosgrove Soothing Staff | — | undequip.eqg | eqg-only (new item) |
| IT11483 | Widow's Fang / Clay-Bladed Dagger / Chamber Etching Spike | — | undequip.eqg | eqg-only (new item) |
| IT11484 | Emberstrike / Lance of Emptiness / Etherpiercer Javelin | — | undequip.eqg | eqg-only (new item) |
| IT11485 | Brass Knuckles / Armsman's Knuckles / Sandstone Knuckles | — | undequip.eqg | eqg-only (new item) |
| IT11486 | Shield of Abstruse Power | — | undequip.eqg | eqg-only (new item) |
| IT11487 | Burning Heart of Slag / Talisman of the Bloodsworn / Ignited Heart of the Firebug | — | undequip.eqg | eqg-only (new item) |
| IT11488 | Fist of Shadow / Touch of the Forgotten / Burning Ash of the Lake | — | undequip.eqg | eqg-only (new item) |
| IT11489 | Poisonheart / Gravity Core / Kranigan's Fist | — | undequip.eqg | eqg-only (new item) |
| IT11490 | Sunburst Tower Shield / Alaran Stalwart Shield / Qeynos Stalwart Shield | — | undequip.eqg | eqg-only (new item) |
| IT11491 | Ornamental Iron Shield | — | undequip.eqg | eqg-only (new item) |
| IT11492 | Qeynos Templar Shield / Ornate Safeguard of the Autarchians | — | undequip.eqg | eqg-only (new item) |
| IT11493 | Shield of Rituals / Magmatic Deflector / Shiny Brass Shield | — | undequip.eqg | eqg-only (new item) |
| IT11494 | Anoh Round Shield / Spore Encrusted Shield / Qeynos Courageous Shield | — | undequip.eqg | eqg-only (new item) |
| IT11495 | Shining Savior's Rampart / Round Shield of Cleansing | — | undequip.eqg | eqg-only (new item) |
| IT11496 | Flayguard / Crystalshell Buckler / Shield of Coiled Terror | — | undequip.eqg | eqg-only (new item) |
| IT11497 | Spiked Guard / Grekenplate Shield / The End of Suffering | — | undequip.eqg | eqg-only (new item) |
| IT11498 | Crescent Staff of the Priestess | — | undequip.eqg | eqg-only (new item) |
| IT11499 | Unneksteel Sword / Rose of the Handmaiden | — | undequip.eqg | eqg-only (new item) |
| IT11500 | Cliknar Limb Flute | gequip3.s3d | — | modern .s3d only |
| IT11501 | Drum / Hand Drum / Sharkskin Drum | gequip3.s3d | — | modern .s3d only |
| IT11502 | Lute / Mandolin / Gypsy Lute | gequip4.s3d | — | modern .s3d only |
| IT11520 | Unneksteel Dagger / Deathrose Stiletto / Stylet of Sacrificed Desires | — | undequip.eqg | eqg-only (new item) |
| IT11521 | Underfoot Defenders Rusty Longsword | — | undequip.eqg | eqg-only (new item) |
| IT11522 |  | — | undequip.eqg | eqg-only (new item) |
| IT11523 | Axe of the Savage / Bile-Etched War Axe | — | undequip.eqg | eqg-only (new item) |
| IT11524 | Spectral Bloodsword | — | undequip.eqg | eqg-only (new item) |
| IT11525 | August Infused Phantasmic Dagger / August Uninfused Phantasmic Dagger | — | undequip.eqg | eqg-only (new item) |
| IT11526 | Fine Lockpicks / Wooden Fishing Tackle / City of Bronze Equipment Chest | — | undequip.eqg | eqg-only (new item) |
| IT11527 | Hard-bristled Brush / Daelai's Dandy Brush / Khati Sha's Favorite Brush | — | undequip.eqg | eqg-only (new item) |
| IT11528 |  | — | undequip.eqg | eqg-only (new item) |
| IT11529 |  | — | undequip.eqg | eqg-only (new item) |
| IT11532 | Heart of the Underfoot Hand Axe / Knight's Heart of the Underfoot Hand Axe | — | undequip.eqg | eqg-only (new item) |
| IT11533 | Heart of the Underfoot Mace / Holy Heart of the Underfoot Mace | — | undequip.eqg | eqg-only (new item) |
| IT11534 | Heart of the Underfoot Battle Staff | — | undequip.eqg | eqg-only (new item) |
| IT11535 | Heart of the Underfoot Hatchet Knife | — | undequip.eqg | eqg-only (new item) |
| IT11536 | Heart of the Underfoot Lance | — | undequip.eqg | eqg-only (new item) |
| IT11537 | Heart of the Underfoot Katar | — | undequip.eqg | eqg-only (new item) |
| IT11538 |  | — | undequip.eqg | eqg-only (new item) |
| IT11590 | Heart of the First Hand Axe / Warmonger's Heart of the First Hand Axe | — | undequip.eqg | eqg-only (new item) |
| IT11591 | Heart of the First Mace / Holy Heart of the First Mace | — | undequip.eqg | eqg-only (new item) |
| IT11592 | Heart of the First Battle Staff | — | undequip.eqg | eqg-only (new item) |
| IT11593 | Heart of the First Hatchet Knife | — | undequip.eqg | eqg-only (new item) |
| IT11594 | Heart of the First Lance | — | undequip.eqg | eqg-only (new item) |
| IT11595 | Heart of the First Katar | — | undequip.eqg | eqg-only (new item) |
| IT11596 | Baronth, Defender of Law | — | undequip.eqg | eqg-only (new item) |
| IT11680 | Displacer Blade / Spirit Sword of the Slain | — | hotequip.eqg | eqg-only (new item) |
| IT11681 | Waspwing Cleaver | — | hotequip.eqg | eqg-only (new item) |
| IT11682 | Waspwing Slicer | — | hotequip.eqg | eqg-only (new item) |
| IT11683 | Sword of the Specter / Heroic Sword of the Specter | — | hotequip.eqg | eqg-only (new item) |
| IT11684 | August Infused Magic Blade Greataxe / August Uninfused Magic Blade Greataxe | — | hotequip.eqg | eqg-only (new item) |
| IT11685 | Heroic Greataxe of Rage / Greataxe of Fervent Rage | — | hotequip.eqg | eqg-only (new item) |
| IT11686 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11687 | Phantasmic Morningstar | — | hotequip.eqg | eqg-only (new item) |
| IT11688 | Empyrean Hammer | — | hotequip.eqg | eqg-only (new item) |
| IT11689 | Polymorph Wand: Flame Telmira | — | hotequip.eqg | eqg-only (new item) |
| IT11690 | Cane of Feral Essence | — | hotequip.eqg | eqg-only (new item) |
| IT11691 | Eclipse | — | hotequip.eqg | eqg-only (new item) |
| IT11692 | Vicious Smasher | — | hotequip.eqg | eqg-only (new item) |
| IT11693 | Ethereal Focus Blade / Ethernere Assassin's Dagger | — | hotequip.eqg | eqg-only (new item) |
| IT11694 | Minadra's Sting / Quilaztli's Cutlass | — | hotequip.eqg | eqg-only (new item) |
| IT11695 | Stolen Spear of the Soothsayer | — | hotequip.eqg | eqg-only (new item) |
| IT11696 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11697 | Zonoraz' Impaler | — | hotequip.eqg | eqg-only (new item) |
| IT11698 | Jagged Metal Knuckles | — | hotequip.eqg | eqg-only (new item) |
| IT11699 | Kiss of the Serpent | — | hotequip.eqg | eqg-only (new item) |
| IT11700 | The Alpha's Claw | — | hotequip.eqg | eqg-only (new item) |
| IT11701 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11702 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11703 | August Infused Glowbow / August Uninfused Glowbow | — | hotequip.eqg | eqg-only (new item) |
| IT11704 | Heroic Guard / Guard of Tangible Thoughts | — | hotequip.eqg | eqg-only (new item) |
| IT11705 | Shield of Fangs | — | hotequip.eqg | eqg-only (new item) |
| IT11706 | Heroic Bulwark / Fearless Bulwark | — | hotequip.eqg | eqg-only (new item) |
| IT11707 | Adamantine Spellblade | — | hotequip.eqg | eqg-only (new item) |
| IT11708 | Axe of Odium / Wicked Axe of Deconstruction | — | hotequip.eqg | eqg-only (new item) |
| IT11709 | Klaknak Falchion / Savage Spiritblade | — | hotequip.eqg | eqg-only (new item) |
| IT11710 | Ianthine Holy Sword / Gixblat's Sword of Massacre | — | hotequip.eqg | eqg-only (new item) |
| IT11711 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11712 | Barrier Breaker / Nightmarish Greataxe / Mindleech-Touched Axe | — | hotequip.eqg | eqg-only (new item) |
| IT11713 | Dread's Blade / Warlord's Shadowy Halberd | — | hotequip.eqg | eqg-only (new item) |
| IT11714 | Headcracker / Mace of Dark Dreams | — | hotequip.eqg | eqg-only (new item) |
| IT11715 | Dyalgem's Hammer / Spectral Darkhammer | — | hotequip.eqg | eqg-only (new item) |
| IT11716 | Possessed Rod | — | hotequip.eqg | eqg-only (new item) |
| IT11717 | Chilling Short Staff / Maeder's Staff of Station | — | hotequip.eqg | eqg-only (new item) |
| IT11718 | Umbrage / Ethercharged Quarterstaff | — | hotequip.eqg | eqg-only (new item) |
| IT11719 | Maul of Shadows / Terror's Hammer | — | hotequip.eqg | eqg-only (new item) |
| IT11720 | Darkdream Dagger / Reaver's Killing Blade | — | hotequip.eqg | eqg-only (new item) |
| IT11721 | Sorcerer's Blade / Otherworldly Blade / Shadow Warden's Rapier | — | hotequip.eqg | eqg-only (new item) |
| IT11722 | Omander's Spiritfork / Spear of the Eagle Eye | — | hotequip.eqg | eqg-only (new item) |
| IT11723 | Ritual Impaler | — | hotequip.eqg | eqg-only (new item) |
| IT11724 | Shieldbearer's Lance | — | hotequip.eqg | eqg-only (new item) |
| IT11725 | Dream Destroyer's Hand | — | hotequip.eqg | eqg-only (new item) |
| IT11726 | Rotslicer / Demon's Horn | — | hotequip.eqg | eqg-only (new item) |
| IT11727 | Mesmerizing Ripper | — | hotequip.eqg | eqg-only (new item) |
| IT11728 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11729 | Amaranthine Shield / Phoboplasmic Shield | — | hotequip.eqg | eqg-only (new item) |
| IT11730 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11731 | Warped Chitin Bow / Kromrif's Stringless Bow of Rot | — | hotequip.eqg | eqg-only (new item) |
| IT11732 | Twisted Dream Shield | — | hotequip.eqg | eqg-only (new item) |
| IT11733 | Warleader's Shield | — | hotequip.eqg | eqg-only (new item) |
| IT11734 | Silhouette, The Mutineer's Sword | — | hotequip.eqg | eqg-only (new item) |
| IT11735 | Mournful Axe of Trapped Souls | — | hotequip.eqg | eqg-only (new item) |
| IT11736 | Dreamscape Cutlass / Scimitar of Elysian Dreams | — | hotequip.eqg | eqg-only (new item) |
| IT11737 | Dreamslicer | — | hotequip.eqg | eqg-only (new item) |
| IT11738 | Celestial Labrys | — | hotequip.eqg | eqg-only (new item) |
| IT11739 | Verecund's Crescent of Light | — | hotequip.eqg | eqg-only (new item) |
| IT11740 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11741 | Mace of Imagination | — | hotequip.eqg | eqg-only (new item) |
| IT11742 | Al'Kabor's Pestle | — | hotequip.eqg | eqg-only (new item) |
| IT11743 | Wand of Uncertain Dreams | — | hotequip.eqg | eqg-only (new item) |
| IT11744 | Thrum / Baton of Trapped Souls | — | hotequip.eqg | eqg-only (new item) |
| IT11745 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11746 | Soulsmasher | — | hotequip.eqg | eqg-only (new item) |
| IT11747 | Glittering Gutripper | — | hotequip.eqg | eqg-only (new item) |
| IT11748 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11749 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11750 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11751 | Ethereal Dreamspear | — | hotequip.eqg | eqg-only (new item) |
| IT11752 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11753 | Luminous Dreampiercer | — | hotequip.eqg | eqg-only (new item) |
| IT11754 | Majestic Dreamclaws | — | hotequip.eqg | eqg-only (new item) |
| IT11755 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11756 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11757 | Fairy Tale Longbow | — | hotequip.eqg | eqg-only (new item) |
| IT11758 | Fanciful Dreamguard | — | hotequip.eqg | eqg-only (new item) |
| IT11759 | Iridescent Shield of War | — | hotequip.eqg | eqg-only (new item) |
| IT11760 | Shield of the Deepwater Knights | — | hotequip.eqg | eqg-only (new item) |
| IT11761 | Eclectic Dreamblade / Withering Shade Blade / Insidionite Shortsword | — | hotequip.eqg | eqg-only (new item) |
| IT11762 | Vernus, Axe of Shattered Dreams | — | hotequip.eqg | eqg-only (new item) |
| IT11763 | Dreamshadow Blade | — | hotequip.eqg | eqg-only (new item) |
| IT11764 | Dreamfall / Claymore of Dreams | — | hotequip.eqg | eqg-only (new item) |
| IT11765 | Crescent of Malign Fantasy | — | hotequip.eqg | eqg-only (new item) |
| IT11766 | Dreamshadow Greataxe | — | hotequip.eqg | eqg-only (new item) |
| IT11767 | Runewing Halberd | — | hotequip.eqg | eqg-only (new item) |
| IT11768 | Dreamshadow Mace | — | hotequip.eqg | eqg-only (new item) |
| IT11769 | Hatemaul of Wretched Paineel | — | hotequip.eqg | eqg-only (new item) |
| IT11770 | Somnolescent Wand | — | hotequip.eqg | eqg-only (new item) |
| IT11771 | Ornate Dreamstopper | — | hotequip.eqg | eqg-only (new item) |
| IT11772 | Archstave of Terris-Thule | — | hotequip.eqg | eqg-only (new item) |
| IT11773 | Darkdream Doomhammer / Castle Guardians' Foehammer | — | hotequip.eqg | eqg-only (new item) |
| IT11774 | The Dagger Pernicious | — | hotequip.eqg | eqg-only (new item) |
| IT11775 | Leypse Piercer | — | hotequip.eqg | eqg-only (new item) |
| IT11776 | Midnight Star | — | hotequip.eqg | eqg-only (new item) |
| IT11777 | Dysrension, Spear of the Archgod | — | hotequip.eqg | eqg-only (new item) |
| IT11778 | Dreamstaff of Reverie | — | hotequip.eqg | eqg-only (new item) |
| IT11779 | Sleeper's Embrace | — | hotequip.eqg | eqg-only (new item) |
| IT11780 | Scalebone, Memoire of Tynn | — | hotequip.eqg | eqg-only (new item) |
| IT11781 | Maleg's Claws | — | hotequip.eqg | eqg-only (new item) |
| IT11782 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11783 | Dreamforged Buckler | — | hotequip.eqg | eqg-only (new item) |
| IT11784 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11785 | Lenquelle's Longbow | — | hotequip.eqg | eqg-only (new item) |
| IT11786 | Somnolescent Doomshingle | — | hotequip.eqg | eqg-only (new item) |
| IT11787 | Dreamforged Shield / Umbrashield of Refracted Sorrow | — | hotequip.eqg | eqg-only (new item) |
| IT11788 | Alliance Longbow / Treantwood Longbow / Fieldsplitter Shortbow | — | hotequip.eqg | eqg-only (new item) |
| IT11789 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11790 | Bow of the Warder | — | hotequip.eqg | eqg-only (new item) |
| IT11791 | Gouzahstring Naperion Bow / Longbow of Time's Reflection | — | hotequip.eqg | eqg-only (new item) |
| IT11792 | Heartwood Bow | — | hotequip.eqg | eqg-only (new item) |
| IT11793 | Mammoth Bone Bow / Regal Infused Recurve Bow / Regal Uninfused Recurve Bow | — | hotequip.eqg | eqg-only (new item) |
| IT11794 | Maraca Familiar / Underythmic Gourd / Shiliskin Metamagic Totem | — | hotequip.eqg | eqg-only (new item) |
| IT11795 | Baton of Office / Visibility Totem / Tribal Song Totem | — | hotequip.eqg | eqg-only (new item) |
| IT11796 | Bleached Totem / Totem of Souls / Whitened Totem | — | hotequip.eqg | eqg-only (new item) |
| IT11797 | Stone Idol / Frozen Totem / Calcified Rage | — | hotequip.eqg | eqg-only (new item) |
| IT11798 | Bellikos Replica / Ritualistic Doll / Whiteshoals' Doll | — | hotequip.eqg | eqg-only (new item) |
| IT11799 | Doll of Tunare / Mad Child's Doll / Badly Chewed Doll | — | hotequip.eqg | eqg-only (new item) |
| IT11800 | Narcoletus Hacker / Axe of the Woodcutter / Regal Infused Shimmer Axe | — | hotequip.eqg | eqg-only (new item) |
| IT11801 | Jumping Axe / Heroic Jumping Axe | — | hotequip.eqg | eqg-only (new item) |
| IT11802 | Brutal Enforcer / Etherwood Greatclub / Regal Infused Spiked Cudgel | — | hotequip.eqg | eqg-only (new item) |
| IT11803 | Fink's Veto / Heroic Scarlet Rod / Intricate Scarlet Rod | — | hotequip.eqg | eqg-only (new item) |
| IT11804 | Darkdreamer's Rod / Heroic Darkdreamer's Rod | — | hotequip.eqg | eqg-only (new item) |
| IT11805 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11806 | Ethertipped Stilleto / Regal Infused Brawler's Fighting Knife / Regal Uninfused Brawler's Fighting Knife | — | hotequip.eqg | eqg-only (new item) |
| IT11807 | Deathpoint / Heroic Deathpoint | — | hotequip.eqg | eqg-only (new item) |
| IT11808 | Deadman's Battleaxe | — | hotequip.eqg | eqg-only (new item) |
| IT11809 | Furious Spiked Staff / Bouncer's Best Friend / Regal Infused Commander's Warstaff | — | hotequip.eqg | eqg-only (new item) |
| IT11810 | Prickly Ball of Wire / Heroic Spiked Knuckles / Fearful Spiked Knuckles | — | hotequip.eqg | eqg-only (new item) |
| IT11811 | Dreadspike, Talon of the Gorgon | — | hotequip.eqg | eqg-only (new item) |
| IT11812 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11813 | Gardener's Thicket Club | — | hotequip.eqg | eqg-only (new item) |
| IT11814 | Heroic Blade of Souls / Blade of Tortured Souls | — | hotequip.eqg | eqg-only (new item) |
| IT11815 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11816 | Painmaker / Heroic Painmaker | — | hotequip.eqg | eqg-only (new item) |
| IT11817 | Scepter of Smashing | — | hotequip.eqg | eqg-only (new item) |
| IT11818 | Rusty Letter Opener / Dagger of Rotting Flesh | — | hotequip.eqg | eqg-only (new item) |
| IT11819 | Runic Partisan / Regal Infused Spirit-Touched Spear / Regal Uninfused Spirit-Touched Spear | — | hotequip.eqg | eqg-only (new item) |
| IT11820 | Cleaver of Time / Regal Infused Knight's Hooked Greatblade / Regal Uninfused Knight's Hooked Greatblade | — | hotequip.eqg | eqg-only (new item) |
| IT11821 | Steel Repulsion / Hammer of Greatness / House Clock Pendulum Bob | — | hotequip.eqg | eqg-only (new item) |
| IT11822 | Warpwood Darkclaw | — | hotequip.eqg | eqg-only (new item) |
| IT11823 | Syncall's Soul / Icesprite Familiar / Snow Spider Familiar | — | hotequip.eqg | eqg-only (new item) |
| IT11824 | Gilded Bow of the Hunter / Heroic Bow of the Hunter | — | hotequip.eqg | eqg-only (new item) |
| IT11825 | Runic Sword of the Servant | — | hotequip.eqg | eqg-only (new item) |
| IT11826 | Nihilium Scepter / Grelleth's Scepter | — | hotequip.eqg | eqg-only (new item) |
| IT11827 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11828 | Arcane Library Paperweight | — | hotequip.eqg | eqg-only (new item) |
| IT11829 | Mindstain Bow of Onerous Accuracy | — | hotequip.eqg | eqg-only (new item) |
| IT11830 | Summoned: Spectral Shortsword | — | hotequip.eqg | eqg-only (new item) |
| IT11831 | Summoned: Spectral Fireblade | — | hotequip.eqg | eqg-only (new item) |
| IT11832 | Summoned: Spectral Iceblade | — | hotequip.eqg | eqg-only (new item) |
| IT11833 | Summoned: Spectral Ragesword | — | hotequip.eqg | eqg-only (new item) |
| IT11834 | Summoned: Jolting Mindblade | — | hotequip.eqg | eqg-only (new item) |
| IT11835 | Candied Brain / Kraken's Brain / Curfang's Heart | — | hotequip.eqg | eqg-only (new item) |
| IT11836 | Izuliard, Bastardsword of Fear Itself | — | hotequip.eqg | eqg-only (new item) |
| IT11837 | Scimitar of Seasons / Wrathborn Natureblade / Scimitar of the Jungle | — | hotequip.eqg | eqg-only (new item) |
| IT11838 | Warped Baton / Armsman's Rod / Ganborn's Rod of Curses | — | hotequip.eqg | eqg-only (new item) |
| IT11839 | Armsman's Wand / Elementalist Rod of Control | — | hotequip.eqg | eqg-only (new item) |
| IT11840 | Rod of Terror's Essence / Runewand of Necropotence / Terakathis, Arbiter of the Dead | — | hotequip.eqg | eqg-only (new item) |
| IT11841 | Plaguescale Rod / Rod of Isolation / Carved Bone Totem | — | hotequip.eqg | eqg-only (new item) |
| IT11842 | Murderer's Knife / Alluring Ivory Dagger | — | hotequip.eqg | eqg-only (new item) |
| IT11843 | Dagger of the Devoted | — | hotequip.eqg | eqg-only (new item) |
| IT11844 | Dagger of Anguish | — | hotequip.eqg | eqg-only (new item) |
| IT11845 | Hollowfield's Cudgel / Qeynos Templar Scepter / Exarch's Intricate Scepter | — | hotequip.eqg | eqg-only (new item) |
| IT11846 | Sotor's Hammer of Pain | — | hotequip.eqg | eqg-only (new item) |
| IT11847 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11848 | Empyrean Scepter | — | hotequip.eqg | eqg-only (new item) |
| IT11849 | Darkfire Scepter | — | hotequip.eqg | eqg-only (new item) |
| IT11850 | Beast King's Scepter | — | hotequip.eqg | eqg-only (new item) |
| IT11851 | Wand of Dreams | — | hotequip.eqg | eqg-only (new item) |
| IT11852 | Pellucid Naazerite Rod | — | hotequip.eqg | eqg-only (new item) |
| IT11853 | Chattering Nymph Rod / Polymorph Wand: Simple Gnoll / Nocturnus, Rod of the Sleepless | — | hotequip.eqg | eqg-only (new item) |
| IT11854 | Erud's Ensorcelled Cane | — | hotequip.eqg | eqg-only (new item) |
| IT11855 | Dreampiercer | — | hotequip.eqg | eqg-only (new item) |
| IT11856 | Darkening Heartsaw | — | hotequip.eqg | eqg-only (new item) |
| IT11857 | Sclerotic Chronodagger | — | hotequip.eqg | eqg-only (new item) |
| IT11858 | Mace of the Dead | — | hotequip.eqg | eqg-only (new item) |
| IT11859 | Brightforce | — | hotequip.eqg | eqg-only (new item) |
| IT11860 | Mace of Morell-Thule | — | hotequip.eqg | eqg-only (new item) |
| IT11861 | Stave of the Limper | — | hotequip.eqg | eqg-only (new item) |
| IT11862 | Blessed Scepter of Prexus | — | hotequip.eqg | eqg-only (new item) |
| IT11863 | Scepter of Totality | — | hotequip.eqg | eqg-only (new item) |
| IT11864 | Distraught Tendril | — | hotequip.eqg | eqg-only (new item) |
| IT11865 | Iron Tekko* / Scaled Fist | — | hotequip.eqg | eqg-only (new item) |
| IT11866 | Mastelyn's Knuckles / Regal Infused Bladed Knuckle Dusters / Regal Uninfused Bladed Knuckle Dusters | — | hotequip.eqg | eqg-only (new item) |
| IT11867 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11868 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11869 | Preener's Kidgloves | — | hotequip.eqg | eqg-only (new item) |
| IT11870 | Dreadiron Knuckles | — | hotequip.eqg | eqg-only (new item) |
| IT11871 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11872 | Ferrous Dreamshield / Shield of Spiteful Dreams | — | hotequip.eqg | eqg-only (new item) |
| IT11873 | Dreamscale / Knight Captain's Shield / Dabner's Shield of Insight | — | hotequip.eqg | eqg-only (new item) |
| IT11874 | Shiverback Shield / Chrysalite Shield of Amiability / Spireward, Guard of the Alendine | — | hotequip.eqg | eqg-only (new item) |
| IT11875 | Unending Slumber / Enthralling Barrier | — | hotequip.eqg | eqg-only (new item) |
| IT11876 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11880 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11881 |  | — | hotequip.eqg | eqg-only (new item) |
| IT11882 | Hurpuc Fruit / Poisoned Apple | — | hotequip.eqg | eqg-only (new item) |
| IT11908 | Thule Family Rivalry | — | hotequip.eqg | eqg-only (new item) |
| IT11909 | The House of Thule | — | hotequip.eqg | eqg-only (new item) |
| IT12020 | Child's Play Santug Claugg's Cudgel Ornament | — | voaequip.eqg | eqg-only (new item) |
| IT12100 | Degmar Ceremonial Hammer | — | voaequip.eqg | eqg-only (new item) |
| IT12101 | Summoned: Manaforged Shortsword | — | voaequip.eqg | eqg-only (new item) |
| IT12102 | Summoned: Manaforged Fireblade | — | voaequip.eqg | eqg-only (new item) |
| IT12103 | Summoned: Manaforged Iceblade | — | voaequip.eqg | eqg-only (new item) |
| IT12104 | Summoned: Manaforged Ragesword | — | voaequip.eqg | eqg-only (new item) |
| IT12105 | August Infused Blade of Solace / Summoned: Manaforged Mindblade / August Uninfused Blade of Solace | — | voaequip.eqg | eqg-only (new item) |
| IT12106 | Rod of Arcane Transvergence | — | voaequip.eqg | eqg-only (new item) |
| IT12107 | Crystal-Bladed Sword / Sword of an Unknown Victim / Glorious Infused Serrated Scimitar | — | voaequip.eqg | eqg-only (new item) |
| IT12108 | Steel Seed Blade / Edge of Punishment / Glorious Infused Knight's Fade Sword | — | voaequip.eqg | eqg-only (new item) |
| IT12109 | Vermin Slicer / Axe of the Exalted | — | voaequip.eqg | eqg-only (new item) |
| IT12110 | Huntsman Halberd / Husk Sharpened Axe / Finalizer, the Record Master's Assistant | — | voaequip.eqg | eqg-only (new item) |
| IT12111 | Greatblade of the Turncoat / Bastardsword of Horrifying Impurity / Glorious Infused Knight's Greatblade | — | voaequip.eqg | eqg-only (new item) |
| IT12112 | Hardened Battle Axe / Glorious Infused Jeweled Great Axe / Glorious Uninfused Jeweled Great Axe | — | voaequip.eqg | eqg-only (new item) |
| IT12113 | Glorious Infused Black Scythe / Glorious Uninfused Black Scythe / Electrified Scythe of the Whipping Wind | — | voaequip.eqg | eqg-only (new item) |
| IT12114 | Poleaxe of Order / Glorious Infused Fauchard / Glorious Uninfused Fauchard | — | voaequip.eqg | eqg-only (new item) |
| IT12115 | Scepter of the Rat Queen / Agralta's Cudgel of Merit / Glorious Infused Reinforced Barbed Mace | — | voaequip.eqg | eqg-only (new item) |
| IT12116 | Mace of Forgiveness / Flanged Argathian Mace / Regal Infused Tiger Mace | — | voaequip.eqg | eqg-only (new item) |
| IT12117 | Hunger Tenderizer / Shadowborn Hammer / Glorious Infused Flame Sealed Hammer | — | voaequip.eqg | eqg-only (new item) |
| IT12118 | Enraged Hammer / Kalkek's Warhammer | — | voaequip.eqg | eqg-only (new item) |
| IT12119 | Battlesteel Hammer / Glorious Infused Hooked Hammer / Glorious Uninfused Hooked Hammer | — | voaequip.eqg | eqg-only (new item) |
| IT12120 | Amorphous Wand of the Vine Tender | — | voaequip.eqg | eqg-only (new item) |
| IT12121 | Tendros' Healing-Stick | — | voaequip.eqg | eqg-only (new item) |
| IT12122 | Spernal's Spiraled Sceptre | — | voaequip.eqg | eqg-only (new item) |
| IT12123 | Energetic Rod / Regal Infused Battle Mage's Baton / Regal Uninfused Battle Mage's Baton | — | voaequip.eqg | eqg-only (new item) |
| IT12124 | Keramar's Wand | — | voaequip.eqg | eqg-only (new item) |
| IT12125 | Rod of the Shieldbearer / Rod of the Scattered Elements / Glorious Infused Platinum Runed Staff | — | voaequip.eqg | eqg-only (new item) |
| IT12126 | Archon's Battlestaff / Soldier's Weighted Staff / Glorious Infused Shimmering Staff | — | voaequip.eqg | eqg-only (new item) |
| IT12127 | Bone Hafted Maul / Maul of the Maddened / Glorious Infused Spiked Maul | — | voaequip.eqg | eqg-only (new item) |
| IT12128 | Venomtooth / Bloodsteel Blade / Glorious Infused Knotched Dagger | — | voaequip.eqg | eqg-only (new item) |
| IT12129 | Backripper / Well Balanced Throwing Knife | — | voaequip.eqg | eqg-only (new item) |
| IT12130 | Polymorph Wand: Flood Telmira / Simple Alaran Sacrificial Dagger / Glorious Infused Spellblade Dagger | — | voaequip.eqg | eqg-only (new item) |
| IT12131 | Steelcore Dagger / Skean of Cerebral Extraction | — | voaequip.eqg | eqg-only (new item) |
| IT12132 | Blood Spirit Touched Dagger | — | voaequip.eqg | eqg-only (new item) |
| IT12133 | Rapier of Legends / Stiletto of the Studious / Regal Infused Viper Rapier | — | voaequip.eqg | eqg-only (new item) |
| IT12134 | Runed Hunting Spear / Archon's Short Lance / Glorious Infused Blood-Etched Spear | — | voaequip.eqg | eqg-only (new item) |
| IT12135 | Envenomed Goral-Prod | — | voaequip.eqg | eqg-only (new item) |
| IT12136 | Clampgrit's Pronged Spear / Glorious Infused Gem-Tipped Spear / Glorious Uninfused Gem-Tipped Spear | — | voaequip.eqg | eqg-only (new item) |
| IT12137 | Braxi-Bone Backbreaker / Erillion Battle Gauntlets / Glorious Infused Lined Sap Gloves | — | voaequip.eqg | eqg-only (new item) |
| IT12138 | Spiked Rage / Bonemeal's Shield-Bladed Katar / Glorious Infused Razor-Edged Ulak | — | voaequip.eqg | eqg-only (new item) |
| IT12139 | Mourning Spikes / Claws of the Bane / Regal Infused Heartblood Ulak | — | voaequip.eqg | eqg-only (new item) |
| IT12140 |  | — | voaequip.eqg | eqg-only (new item) |
| IT12141 | Erilon Soldier Bow / Glorious Infused Recurve Bow / Glorious Uninfused Recurve Bow | — | voaequip.eqg | eqg-only (new item) |
| IT12142 | Hralkt, Darkbow of Unwilling Servitude | — | voaequip.eqg | eqg-only (new item) |
| IT12143 | Illum's Cross / Steelcore Shield / Spiritblood Shield | — | voaequip.eqg | eqg-only (new item) |
| IT12144 | Reviler's Steelwall / Riffmaz's Urn-Shield | — | voaequip.eqg | eqg-only (new item) |
| IT12145 | Ancient Alaran Shield / Bulwark of Suspended Shade | — | voaequip.eqg | eqg-only (new item) |
| IT12146 | Shield of Grievance / Guard of the Emissary / Deepblade's Longshield | — | voaequip.eqg | eqg-only (new item) |
| IT12147 | Argath Defender's Sword / Decree, Blade of Justice | — | voaequip.eqg | eqg-only (new item) |
| IT12148 | Cutter / Thralogon, Blade of the Deific Defender | — | voaequip.eqg | eqg-only (new item) |
| IT12149 | Deroth, Enforcer / Blade of Witless Abandon | — | voaequip.eqg | eqg-only (new item) |
| IT12150 | Wis'Ena, Cloudcutter | — | voaequip.eqg | eqg-only (new item) |
| IT12151 | Pride, Wave of Resplendence | — | voaequip.eqg | eqg-only (new item) |
| IT12152 | Angered Blade of Alsa Thel | — | voaequip.eqg | eqg-only (new item) |
| IT12153 | Hathor, Scythe of Punishment | — | voaequip.eqg | eqg-only (new item) |
| IT12154 | Plated Halberd | — | voaequip.eqg | eqg-only (new item) |
| IT12155 | Thunder, Mace of Conviction | — | voaequip.eqg | eqg-only (new item) |
| IT12156 | Din, Breaker of Harmonies | — | voaequip.eqg | eqg-only (new item) |
| IT12157 | Riptide | — | voaequip.eqg | eqg-only (new item) |
| IT12158 | Hammer of the Deep | — | voaequip.eqg | eqg-only (new item) |
| IT12159 | Edict of Battle | — | voaequip.eqg | eqg-only (new item) |
| IT12160 | Vreklen, Spire of Arcane Extraction | — | voaequip.eqg | eqg-only (new item) |
| IT12161 | Cudgel of Forgetful Sacrifice | — | voaequip.eqg | eqg-only (new item) |
| IT12162 | Ethic, Scepter of Law | — | voaequip.eqg | eqg-only (new item) |
| IT12163 | Rod of Battle | — | voaequip.eqg | eqg-only (new item) |
| IT12164 | Steel Rod | — | voaequip.eqg | eqg-only (new item) |
| IT12165 | Orb of Ladrys | — | voaequip.eqg | eqg-only (new item) |
| IT12166 | Greatstaff of the Domain / Staff of Unforgivable Folly | — | voaequip.eqg | eqg-only (new item) |
| IT12167 | Maul of Plates | — | voaequip.eqg | eqg-only (new item) |
| IT12168 | Domain Hunter's Knife | — | voaequip.eqg | eqg-only (new item) |
| IT12169 | Arc, Incisor | — | voaequip.eqg | eqg-only (new item) |
| IT12170 |  | — | voaequip.eqg | eqg-only (new item) |
| IT12171 | Battle Avatar's Shard | — | voaequip.eqg | eqg-only (new item) |
| IT12172 | Seltan, Blade of Beloth | — | voaequip.eqg | eqg-only (new item) |
| IT12173 | Syrsrek, Spike of the Spiritward | — | voaequip.eqg | eqg-only (new item) |
| IT12174 | Battle Prod / Ornate Hunting Spear | — | voaequip.eqg | eqg-only (new item) |
| IT12175 |  | — | voaequip.eqg | eqg-only (new item) |
| IT12176 | Rebuttal, Ender of Arguments | — | voaequip.eqg | eqg-only (new item) |
| IT12177 | Ring of the Battle Avatar | — | voaequip.eqg | eqg-only (new item) |
| IT12178 | Vine Covered Spike / Kral`Sek, Nature's Brutality | — | voaequip.eqg | eqg-only (new item) |
| IT12179 | Beast King's Claws | — | voaequip.eqg | eqg-only (new item) |
| IT12180 |  | — | voaequip.eqg | eqg-only (new item) |
| IT12181 | Battle Avatar's Rib | — | voaequip.eqg | eqg-only (new item) |
| IT12182 | Thrum, Bow of the Stars | — | voaequip.eqg | eqg-only (new item) |
| IT12183 | Lawkeeper's Bulwark / Bulwark of the Eternally Devout | — | voaequip.eqg | eqg-only (new item) |
| IT12184 | Battle Soother's Guard / En'Ceri, Ward of Sound | — | voaequip.eqg | eqg-only (new item) |
| IT12185 | Fold, Shield of Sound | — | voaequip.eqg | eqg-only (new item) |
| IT12186 | Plated Heater / Crest of the Faithful Fallen | — | voaequip.eqg | eqg-only (new item) |
| IT12187 | Sea Ranger's Blade / Arulien's Broken Sword / Hooked Sword of the Holy | — | voaequip.eqg | eqg-only (new item) |
| IT12188 | Reaver of the Deep | — | voaequip.eqg | eqg-only (new item) |
| IT12189 | Aeyayen, the Deveiner / Hadal Blade of the Deep | — | voaequip.eqg | eqg-only (new item) |
| IT12190 | Mollen's Divining Rod / Ra`Argt, the Shell-Crusher | — | voaequip.eqg | eqg-only (new item) |
| IT12191 | Rettun's Cudgel | — | voaequip.eqg | eqg-only (new item) |
| IT12192 | Rod of the Waves / Polymorph Wand: Armored Shiliskin | — | voaequip.eqg | eqg-only (new item) |
| IT12193 | Wavecrasher's Conch-Mace | — | voaequip.eqg | eqg-only (new item) |
| IT12194 | Thale's Trophy / Sarith Waterknife | — | voaequip.eqg | eqg-only (new item) |
| IT12195 | Kyzer's Ritual Dagger | — | voaequip.eqg | eqg-only (new item) |
| IT12196 | Spiral Spear | — | voaequip.eqg | eqg-only (new item) |
| IT12197 |  | — | voaequip.eqg | eqg-only (new item) |
| IT12198 | Deadly Lambis / Spiny Whelk-Fist | — | voaequip.eqg | eqg-only (new item) |
| IT12199 |  | — | voaequip.eqg | eqg-only (new item) |
| IT12200 | Sarith Scout Bow | — | voaequip.eqg | eqg-only (new item) |
| IT12201 | Enormous Shell Guard / Shell-Shield of the Sacred | — | voaequip.eqg | eqg-only (new item) |
| IT12202 | Battlemage's Shield | — | voaequip.eqg | eqg-only (new item) |
| IT12203 | Waveshard | — | voaequip.eqg | eqg-only (new item) |
| IT12204 | Heart of the Hadal | — | voaequip.eqg | eqg-only (new item) |
| IT12205 | Ocean-Formed Greatblade | — | voaequip.eqg | eqg-only (new item) |
| IT12206 | Oseka's Truth / Scepter of Oseka | — | voaequip.eqg | eqg-only (new item) |
| IT12207 | Tidalwave | — | voaequip.eqg | eqg-only (new item) |
| IT12208 | Wavering Water Pearl / Polymorph Wand: Hadal Templar | — | voaequip.eqg | eqg-only (new item) |
| IT12209 | Wavestruck Maul / Greathammer of the Currents | — | voaequip.eqg | eqg-only (new item) |
| IT12210 |  | — | voaequip.eqg | eqg-only (new item) |
| IT12211 | Ritual Blade of the Tides | — | voaequip.eqg | eqg-only (new item) |
| IT12212 | Spinedriller | — | voaequip.eqg | eqg-only (new item) |
| IT12213 |  | — | voaequip.eqg | eqg-only (new item) |
| IT12214 | Conch of the Tides / Spikeshell, Wave of Solutions | — | voaequip.eqg | eqg-only (new item) |
| IT12215 |  | — | voaequip.eqg | eqg-only (new item) |
| IT12216 | Crab Point / Greatbow of the Waves | — | voaequip.eqg | eqg-only (new item) |
| IT12217 | Sand Dollar Shield | — | voaequip.eqg | eqg-only (new item) |
| IT12218 | Sarith Emblazoned Shield | — | voaequip.eqg | eqg-only (new item) |
| IT12219 | Brightedge / Brilliant Sword of Songs / Resplendent Sword Ornament | — | voaequip.eqg | eqg-only (new item) |
| IT12220 | Graven Grave-Glaive / Rusty Ceremonial Axe / Resplendent Axe Ornament | — | voaequip.eqg | eqg-only (new item) |
| IT12221 | Zarq's Scrap-Iron Longsword / Resplendent Great Sword Ornament | — | voaequip.eqg | eqg-only (new item) |
| IT12222 | Warmace of the High Guard | — | voaequip.eqg | eqg-only (new item) |
| IT12223 | Resplendent Hammer Ornament / Deteriorating Hammer of the Zombified | — | voaequip.eqg | eqg-only (new item) |
| IT12224 | Harbinger's Staff / Mindlock's Ritual-Staff | — | voaequip.eqg | eqg-only (new item) |
| IT12225 | Julazir's Brazen Gavel / Regal Infused Siege Maul / Resplendent Maul Ornament | — | voaequip.eqg | eqg-only (new item) |
| IT12226 | Dirk of the Groundskeeper / Swift Dagger of the Blacksmith | — | voaequip.eqg | eqg-only (new item) |
| IT12227 | Resplendent Dagger Ornament / Arched Blade of Gleaming Tricor | — | voaequip.eqg | eqg-only (new item) |
| IT12228 | Radiant Shortspear of the Goral | — | voaequip.eqg | eqg-only (new item) |
| IT12229 | Resplendent Spear Ornament / Regal Infused Warhoned Glaive / Gisette's Needle-Pronged Spear | — | voaequip.eqg | eqg-only (new item) |
| IT12230 | Resplendent Katar Ornament / Superb Katar of the Advisor | — | voaequip.eqg | eqg-only (new item) |
| IT12231 | Refracted Steel Longbow / Resplendent Bow Ornament / Ikanyrd, Longbow of Arcane Fortune | — | voaequip.eqg | eqg-only (new item) |
| IT12232 | Tri-Shield of the King / Bronze-Adorned Smallshield / Cijerst's Decayed Targ Shield | — | voaequip.eqg | eqg-only (new item) |
| IT12233 | Resplendent Shield Ornament / Temple Shield of the Servant | — | voaequip.eqg | eqg-only (new item) |
| IT12234 | Splendor / Glory, Badge of Resplendence | — | voaequip.eqg | eqg-only (new item) |
| IT12235 | Resplendent Cleaver | — | voaequip.eqg | eqg-only (new item) |
| IT12236 | Divine Resplendence | — | voaequip.eqg | eqg-only (new item) |
| IT12237 | Radiant Mace | — | voaequip.eqg | eqg-only (new item) |
| IT12238 | Hammer of Cleansing | — | voaequip.eqg | eqg-only (new item) |
| IT12239 | Staff of Clarification / Staff of Glaring Divinity | — | voaequip.eqg | eqg-only (new item) |
| IT12240 | En'Theru, Ender of Crimes | — | voaequip.eqg | eqg-only (new item) |
| IT12241 | Soulbleeder | — | voaequip.eqg | eqg-only (new item) |
| IT12242 | Immaculous, Bore of Purity | — | voaequip.eqg | eqg-only (new item) |
| IT12243 | Piercing Clarity | — | voaequip.eqg | eqg-only (new item) |
| IT12244 |  | — | voaequip.eqg | eqg-only (new item) |
| IT12245 |  | — | voaequip.eqg | eqg-only (new item) |
| IT12246 | Gar'Theru, Bringer of Rewards | — | voaequip.eqg | eqg-only (new item) |
| IT12247 | Absolving Guard / Allu'Gre, Guardian of the Righteous | — | voaequip.eqg | eqg-only (new item) |
| IT12248 | Clarifying Guard | — | voaequip.eqg | eqg-only (new item) |
| IT12259 | Planar Scorched Flail Ornamentation | — | voaequip.eqg | eqg-only (new item) |
| IT12260 | Planar Scorched Combat Staff Ornamentation | — | voaequip.eqg | eqg-only (new item) |
| IT12261 | Planar Scorched Bladed Mace Ornamentation | — | voaequip.eqg | eqg-only (new item) |
| IT12262 | Planar Scorched Glaive Ornamentation | — | voaequip.eqg | eqg-only (new item) |
| IT12263 | Planar Scorched Dueling Blade Ornamentation | — | voaequip.eqg | eqg-only (new item) |
| IT12264 | Planar Scorched Ranseur Ornamentation | — | voaequip.eqg | eqg-only (new item) |
| IT12265 | Planar Scorched Cat Claws Ornamentation | — | voaequip.eqg | eqg-only (new item) |
| IT12266 | Planar Scorched Buckler Ornamentation | — | voaequip.eqg | eqg-only (new item) |
| IT12267 | Planar Scorched Greatbow Ornamentation | — | voaequip.eqg | eqg-only (new item) |
| IT12268 |  | — | voaequip.eqg | eqg-only (new item) |
| IT12301 | Trident of the Triumvirate Replica | — | voaequip.eqg | eqg-only (new item) |
| IT12313 | Leeching Claws of Varinyr | — | voaequip.eqg | eqg-only (new item) |
| IT12314 |  | — | voaequip.eqg | eqg-only (new item) |
| IT12374 | Axe of Splitting Glaciers | — | rofequip.eqg | eqg-only (new item) |
| IT12375 | Sword of the Lone Tarot | — | rofequip.eqg | eqg-only (new item) |
| IT12376 | Galrok's Stick | — | rofequip.eqg | eqg-only (new item) |
| IT12377 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12378 | Crushing Aegis of Dawn | — | rofequip.eqg | eqg-only (new item) |
| IT12379 | Grydon's Blazing Sceptre | — | rofequip.eqg | eqg-only (new item) |
| IT12380 | Velium Spike of Madness | — | rofequip.eqg | eqg-only (new item) |
| IT12381 | Arctic Blade of Enforcing | — | rofequip.eqg | eqg-only (new item) |
| IT12382 | Ry'Gorr Ceremonial Dagger | — | rofequip.eqg | eqg-only (new item) |
| IT12383 | The Frozen's Grudge | — | rofequip.eqg | eqg-only (new item) |
| IT12384 | Kreztik's Blistering Judgment | — | rofequip.eqg | eqg-only (new item) |
| IT12385 | Velium Knuckles of the Pit | — | rofequip.eqg | eqg-only (new item) |
| IT12386 | Velium Staff of Obliteration | — | rofequip.eqg | eqg-only (new item) |
| IT12387 | Frozen Lunatic's Hammer | — | rofequip.eqg | eqg-only (new item) |
| IT12388 | Bow of Nature's Caress | — | rofequip.eqg | eqg-only (new item) |
| IT12389 | Stygian Nightmare's Bow | — | rofequip.eqg | eqg-only (new item) |
| IT12390 | Shield of Greatness | — | rofequip.eqg | eqg-only (new item) |
| IT12391 | Brazen Barricade | — | rofequip.eqg | eqg-only (new item) |
| IT12392 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12393 | Hissing Unsociable Blade | — | rofequip.eqg | eqg-only (new item) |
| IT12394 | Laconic Edged Pincer | — | rofequip.eqg | eqg-only (new item) |
| IT12395 | Voiceless Hound Skean | — | rofequip.eqg | eqg-only (new item) |
| IT12396 | Silent Grimace | — | rofequip.eqg | eqg-only (new item) |
| IT12397 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12398 | Reticent Gar | — | rofequip.eqg | eqg-only (new item) |
| IT12399 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12400 | Dreadful Visage | — | rofequip.eqg | eqg-only (new item) |
| IT12401 | Sticky Dagger | — | rofequip.eqg | eqg-only (new item) |
| IT12402 | The Defiler's Rebuke | — | rofequip.eqg | eqg-only (new item) |
| IT12403 | Jom'Agurd | — | rofequip.eqg | eqg-only (new item) |
| IT12404 | Mace of Life Draining | — | rofequip.eqg | eqg-only (new item) |
| IT12405 | Bow of Grasping Vines | — | rofequip.eqg | eqg-only (new item) |
| IT12406 | Savage Wall | — | rofequip.eqg | eqg-only (new item) |
| IT12407 | Force of Two | — | rofequip.eqg | eqg-only (new item) |
| IT12408 | Sorrow's Bane | — | rofequip.eqg | eqg-only (new item) |
| IT12409 | Gentleman's Sword | — | rofequip.eqg | eqg-only (new item) |
| IT12410 | Strength of Valor / August Infused Military Warsledge / August Uninfused Military Warsledge | — | rofequip.eqg | eqg-only (new item) |
| IT12411 | Mace of the Oasis | — | rofequip.eqg | eqg-only (new item) |
| IT12412 | Plague-borne Wall | — | rofequip.eqg | eqg-only (new item) |
| IT12413 | Dew Scepter | — | rofequip.eqg | eqg-only (new item) |
| IT12414 | Blazing Hammer of Destruction | — | rofequip.eqg | eqg-only (new item) |
| IT12415 | Shifting Shanker | — | rofequip.eqg | eqg-only (new item) |
| IT12416 | Dragoncold Lance | — | rofequip.eqg | eqg-only (new item) |
| IT12417 | Quake Fist | — | rofequip.eqg | eqg-only (new item) |
| IT12418 | War Weilded Grotesque | — | rofequip.eqg | eqg-only (new item) |
| IT12419 | Iceblade Knuckles | — | rofequip.eqg | eqg-only (new item) |
| IT12420 | Vessel of Pain | — | rofequip.eqg | eqg-only (new item) |
| IT12421 | Filth-Covered Shiv | — | rofequip.eqg | eqg-only (new item) |
| IT12422 | Surgeon General | — | rofequip.eqg | eqg-only (new item) |
| IT12423 | Living Axe | — | rofequip.eqg | eqg-only (new item) |
| IT12424 | Strength of Legions | — | rofequip.eqg | eqg-only (new item) |
| IT12425 | Terror's Fist / Fist of Fidelity | — | rofequip.eqg | eqg-only (new item) |
| IT12426 | Panic Attack | — | rofequip.eqg | eqg-only (new item) |
| IT12427 | Sunbeam Bow | — | rofequip.eqg | eqg-only (new item) |
| IT12428 | Shield-Like Remains | — | rofequip.eqg | eqg-only (new item) |
| IT12429 | Snowflake | — | rofequip.eqg | eqg-only (new item) |
| IT12430 | Septic Skewer | — | rofequip.eqg | eqg-only (new item) |
| IT12431 | Frosthewn Truncheon | — | rofequip.eqg | eqg-only (new item) |
| IT12432 | Double Trouble | — | rofequip.eqg | eqg-only (new item) |
| IT12433 | Bonebasher | — | rofequip.eqg | eqg-only (new item) |
| IT12434 | Glory Abound | — | rofequip.eqg | eqg-only (new item) |
| IT12435 | Frosthewn Longspear | — | rofequip.eqg | eqg-only (new item) |
| IT12436 | Venomous Skiver | — | rofequip.eqg | eqg-only (new item) |
| IT12437 | Iceblind Shiv | — | rofequip.eqg | eqg-only (new item) |
| IT12438 | Snowreaver | — | rofequip.eqg | eqg-only (new item) |
| IT12439 | Frosthewn Greathammer | — | rofequip.eqg | eqg-only (new item) |
| IT12440 | Prophetic Cestus | — | rofequip.eqg | eqg-only (new item) |
| IT12441 | Frosthewn Brawl Stick | — | rofequip.eqg | eqg-only (new item) |
| IT12442 | Frosthewn Greatmace | — | rofequip.eqg | eqg-only (new item) |
| IT12443 | Frosthewn Shortbow | — | rofequip.eqg | eqg-only (new item) |
| IT12444 | Iceblind | — | rofequip.eqg | eqg-only (new item) |
| IT12445 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12446 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12447 | Broodsworn Shield | — | rofequip.eqg | eqg-only (new item) |
| IT12448 | Slaughter | — | rofequip.eqg | eqg-only (new item) |
| IT12449 | Durew's Mace of Inverse Logic | — | rofequip.eqg | eqg-only (new item) |
| IT12450 | Dagger of Spectral Disruption | — | rofequip.eqg | eqg-only (new item) |
| IT12451 | Darkspike | — | rofequip.eqg | eqg-only (new item) |
| IT12452 | Silence | — | rofequip.eqg | eqg-only (new item) |
| IT12453 | Unspoken Deliverance | — | rofequip.eqg | eqg-only (new item) |
| IT12454 | Shadeblade | — | rofequip.eqg | eqg-only (new item) |
| IT12455 | Rune of the Unspoken | — | rofequip.eqg | eqg-only (new item) |
| IT12456 | Heartshard | — | rofequip.eqg | eqg-only (new item) |
| IT12457 | Heartstave | — | rofequip.eqg | eqg-only (new item) |
| IT12458 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12459 | Feartwisted Mace | — | rofequip.eqg | eqg-only (new item) |
| IT12460 | Shardheart | — | rofequip.eqg | eqg-only (new item) |
| IT12461 | Heartguard | — | rofequip.eqg | eqg-only (new item) |
| IT12462 | Serviceman's Honor | — | rofequip.eqg | eqg-only (new item) |
| IT12463 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12464 | Chapterhouse Standard | — | rofequip.eqg | eqg-only (new item) |
| IT12465 | Weight of Duty | — | rofequip.eqg | eqg-only (new item) |
| IT12466 | Imposing Law of Xorbb | — | rofequip.eqg | eqg-only (new item) |
| IT12467 | Befallen Noble's Aegis | — | rofequip.eqg | eqg-only (new item) |
| IT12468 | Starrla's Cloudy Pink Scepter of Veeshan | — | rofequip.eqg | eqg-only (new item) |
| IT12469 | Hammer of Dragon's Might | — | rofequip.eqg | eqg-only (new item) |
| IT12470 | Piercing Wrath of the Grounds | — | rofequip.eqg | eqg-only (new item) |
| IT12471 | Enkindling Pike | — | rofequip.eqg | eqg-only (new item) |
| IT12472 | Qulas' Fiery Katar of Insanity | — | rofequip.eqg | eqg-only (new item) |
| IT12473 | Wuran's Staff of Decay | — | rofequip.eqg | eqg-only (new item) |
| IT12474 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12475 | Midasa's Dragon Slayer | — | rofequip.eqg | eqg-only (new item) |
| IT12476 | Poker of Calming | — | rofequip.eqg | eqg-only (new item) |
| IT12477 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12478 | Yymp's Research Cleaver | — | rofequip.eqg | eqg-only (new item) |
| IT12479 | Brutal Frostbite of the Valley Master | — | rofequip.eqg | eqg-only (new item) |
| IT12480 | Bladed Hand of Xorbb | — | rofequip.eqg | eqg-only (new item) |
| IT12481 | Draining Ire of the Glen Liege | — | rofequip.eqg | eqg-only (new item) |
| IT12482 | Arwyn's Greatbow of Valor | — | rofequip.eqg | eqg-only (new item) |
| IT12483 | Baneful Eye Buckler | — | rofequip.eqg | eqg-only (new item) |
| IT12484 | Mug of Eternal Ale / Crystilla's Signature Ale / Zavitar's Ale of Intimidation | — | rofequip.eqg | eqg-only (new item) |
| IT12485 | The Perfect Stout / Crystilla's Special Ale / Frosty Mug of the Observant | — | rofequip.eqg | eqg-only (new item) |
| IT12486 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12487 | Vile Sarnak Brew / All Purpose Glaze | — | rofequip.eqg | eqg-only (new item) |
| IT12488 | Donal's Teacup of Mourning | — | rofequip.eqg | eqg-only (new item) |
| IT12489 | Flask of Liquor / Buccaneer's Canteen | — | rofequip.eqg | eqg-only (new item) |
| IT12490 | Endless Wineskin / Skin of Bitter Wine | — | rofequip.eqg | eqg-only (new item) |
| IT12491 | Liquid Rage / Flask of Rage / Flask of Fire Water | — | rofequip.eqg | eqg-only (new item) |
| IT12492 | Lava Vial / Bottled Rage / Rage of the Widow | — | rofequip.eqg | eqg-only (new item) |
| IT12493 | 5 Dose Essence of Imp / 5 Dose Essence of Orc / 5 Dose Essence of Bear | — | rofequip.eqg | eqg-only (new item) |
| IT12494 | 5 Dose Potion of the Shiliskin / 5 Dose Potion of the Steam Suit / 5 Dose Potion of the Froglok Ghost | — | rofequip.eqg | eqg-only (new item) |
| IT12495 | Silver Fork Ornament | — | rofequip.eqg | eqg-only (new item) |
| IT12496 | Morden's Lucky Knife / Silver Knife Ornament | — | rofequip.eqg | eqg-only (new item) |
| IT12497 | Gluttonous Spoon / Silver Spoon Ornament / Sionachie's Soup Spoon | — | rofequip.eqg | eqg-only (new item) |
| IT12498 | Fancy Drumstick / Drumstick Ornament | — | rofequip.eqg | eqg-only (new item) |
| IT12499 | Liver / Spleen / Tongue | — | rofequip.eqg | eqg-only (new item) |
| IT12500 | Freebooter's Coconut / Coconut Husk Ornament | — | rofequip.eqg | eqg-only (new item) |
| IT12501 | Dreamwalker's Amygdala | — | rofequip.eqg | eqg-only (new item) |
| IT12502 | Amio's Explosives | — | rofequip.eqg | eqg-only (new item) |
| IT12503 | Hammer of Reverence / Hammer of Reverence II / Hammer of Reverence III | — | rofequip.eqg | eqg-only (new item) |
| IT12504 | Summoned: Thalassic Shortsword / Summoned: Frightforged Shortsword | — | rofequip.eqg | eqg-only (new item) |
| IT12505 | Summoned: Thalassic Fireblade / Summoned: Frightforged Fireblade | — | rofequip.eqg | eqg-only (new item) |
| IT12506 | Summoned: Thalassic Mindblade / Summoned: Frightforged Mindblade | — | rofequip.eqg | eqg-only (new item) |
| IT12507 | Summoned: Thalassic Ragesword / Summoned: Frightforged Ragesword | — | rofequip.eqg | eqg-only (new item) |
| IT12508 | Summoned: Thalassic Iceblade / Summoned: Frightforged Iceblade | — | rofequip.eqg | eqg-only (new item) |
| IT12521 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12522 | Imrahil's Claidhmore of Raging Fury | — | rofequip.eqg | eqg-only (new item) |
| IT12523 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12524 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12525 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12526 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12527 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12528 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12529 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12530 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12531 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12532 |  | — | rofequip.eqg | eqg-only (new item) |
| IT12563 | Underdog's Cleaver | — | hofequip.eqg | eqg-only (new item) |
| IT12564 | Indomitable Chopper | — | hofequip.eqg | eqg-only (new item) |
| IT12565 | Flick's Flounder Grabber | — | hofequip.eqg | eqg-only (new item) |
| IT12566 | Pacifying Skullcracker | — | hofequip.eqg | eqg-only (new item) |
| IT12567 |  | — | hofequip.eqg | eqg-only (new item) |
| IT12568 | Swordmaster's Blade | — | hofequip.eqg | eqg-only (new item) |
| IT12569 | The Gourdsmasher | — | hofequip.eqg | eqg-only (new item) |
| IT12570 | The Cat's Paw | — | hofequip.eqg | eqg-only (new item) |
| IT12571 | The Rod of Favoritism | — | hofequip.eqg | eqg-only (new item) |
| IT12572 | Iksar Bone Bow | — | hofequip.eqg | eqg-only (new item) |
| IT12573 | Prototype SH32 | — | hofequip.eqg | eqg-only (new item) |
| IT12574 | Polished Verdant Blade | — | hofequip.eqg | eqg-only (new item) |
| IT12575 | Verdant Curved Blade | — | hofequip.eqg | eqg-only (new item) |
| IT12576 | Verdant Neckcracker | — | hofequip.eqg | eqg-only (new item) |
| IT12577 | Verdant Mace of the Vicar | — | hofequip.eqg | eqg-only (new item) |
| IT12578 | Verdant Heartpiercer | — | hofequip.eqg | eqg-only (new item) |
| IT12579 | Verdant Coiled Blade | — | hofequip.eqg | eqg-only (new item) |
| IT12580 | Verdant Spear of the Shissar | — | hofequip.eqg | eqg-only (new item) |
| IT12581 | Verdant Claws of the Soul | — | hofequip.eqg | eqg-only (new item) |
| IT12582 | Crescent Staff of the Shissar | — | hofequip.eqg | eqg-only (new item) |
| IT12583 | Verdant Serpentine Bow | — | hofequip.eqg | eqg-only (new item) |
| IT12584 | Verdant Shield of the Shissar | — | hofequip.eqg | eqg-only (new item) |
| IT12585 | Moonlight Hatchet | — | hofequip.eqg | eqg-only (new item) |
| IT12586 | Moonlight Shortsword | — | hofequip.eqg | eqg-only (new item) |
| IT12587 | Moonlight Bludgeoner | — | hofequip.eqg | eqg-only (new item) |
| IT12588 | Moonlight Cudgel | — | hofequip.eqg | eqg-only (new item) |
| IT12589 | Moonlight Staff | — | hofequip.eqg | eqg-only (new item) |
| IT12590 | Moonlight Stiletto / August Infused Voidmaster's Dagger / August Uninfused Voidmaster's Dagger | — | hofequip.eqg | eqg-only (new item) |
| IT12591 | Shadowrock Lance | — | hofequip.eqg | eqg-only (new item) |
| IT12592 | Moonlight Blades | — | hofequip.eqg | eqg-only (new item) |
| IT12593 | Moonlight Ceremonial Staff | — | hofequip.eqg | eqg-only (new item) |
| IT12594 | Moonlight Longbow | — | hofequip.eqg | eqg-only (new item) |
| IT12595 | Barrier of Flowing Light | — | hofequip.eqg | eqg-only (new item) |
| IT12596 | Xau Tekar, Blade of Night | — | hofequip.eqg | eqg-only (new item) |
| IT12597 | Vol Tekar, Edge of Light | — | hofequip.eqg | eqg-only (new item) |
| IT12598 | Vius Xiall, Hammer of Dawn | — | hofequip.eqg | eqg-only (new item) |
| IT12599 | Xin Xakreum, Baton of the War Spirit | — | hofequip.eqg | eqg-only (new item) |
| IT12600 | Crys Dator, Wand of Indentured | — | hofequip.eqg | eqg-only (new item) |
| IT12601 | Xau Kaas, Blade of Souls | — | hofequip.eqg | eqg-only (new item) |
| IT12602 | Pli Teka, Lance of Pure Pain | — | hofequip.eqg | eqg-only (new item) |
| IT12603 | Thox Tatrua, Ulak of the Quick Death | — | hofequip.eqg | eqg-only (new item) |
| IT12604 | Xenu Xakra, Staff of Unlife | — | hofequip.eqg | eqg-only (new item) |
| IT12605 | Eom Draues, Longbow of Torment | — | hofequip.eqg | eqg-only (new item) |
| IT12606 | Umbralforge Shield | — | hofequip.eqg | eqg-only (new item) |
| IT12607 |  | — | hofequip.eqg | eqg-only (new item) |
| IT12608 |  | — | hofequip.eqg | eqg-only (new item) |
| IT12609 | The Crippler | — | hofequip.eqg | eqg-only (new item) |
| IT20015 |  | — | hotequip.eqg | eqg-only (new item) |
| IT29250 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29251 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29252 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29350 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29351 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29352 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29354 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29356 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29357 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29359 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29361 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29362 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29363 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29364 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29365 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29366 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29367 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29368 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29369 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29400 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29401 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29402 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29403 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29404 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29405 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29406 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29407 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29408 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29409 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29410 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29411 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29412 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29413 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29414 |  | — | rofequip.eqg | eqg-only (new item) |
| IT29415 |  | — | rofequip.eqg | eqg-only (new item) |
| IT67367 | Shield of Silent Shadow / Scouting Shield of the Da Bashers | gequip4.s3d | — | modern .s3d only |

## Characters & NPCs — replaced global models

Codes defined in more than one archive — a newer one wins, hiding the classic look. The base is the shared `global[N]_chr.s3d`; the override is the per-race `global<race><gender>_chr.s3d` (loaded on demand by the `UseLuclin<Race><Sex>` `eqclient.ini` toggles) or a later shared archive. Creatures like skeletons are force-loaded via `GlobalLoad.txt`.

| code | name | classic / base | overridden by |
|---|---|---|---|
| BAF | Barbarian female | global_chr.s3d | global4_chr.s3d, globalbaf_chr.s3d |
| BAM | Barbarian male | global_chr.s3d | global4_chr.s3d, globalbam_chr.s3d |
| DAF | Dark Elf female | global_chr.s3d | globaldaf_chr.s3d |
| DAM | Dark Elf male | global_chr.s3d | globaldam_chr.s3d |
| DWF | Dwarf female | global_chr.s3d | globaldwf_chr.s3d |
| DWM | Dwarf male | global_chr.s3d | globaldwm_chr.s3d |
| ELF | Wood Elf female | global_chr.s3d | globalelf_chr.s3d |
| ELM | Wood Elf male | global_chr.s3d | globalelm_chr.s3d |
| ERF | Erudite female | global_chr.s3d | globalerf_chr.s3d |
| ERM | Erudite male | global_chr.s3d | globalerm_chr.s3d |
| GNF | Gnome female | global_chr.s3d | globalgnf_chr.s3d |
| GNM | Gnome male | global_chr.s3d | globalgnm_chr.s3d |
| HAF | Half Elf female | global_chr.s3d | globalhaf_chr.s3d |
| HAM | Half Elf male | global_chr.s3d | globalham_chr.s3d |
| HIF | High Elf female | global_chr.s3d | globalhif_chr.s3d |
| HIM | High Elf male | global_chr.s3d | globalhim_chr.s3d |
| HOF | Halfling female | global_chr.s3d | globalhof_chr.s3d |
| HOM | Halfling male | global_chr.s3d | globalhom_chr.s3d |
| HUF | Human female | global_chr.s3d | globalhuf_chr.s3d |
| HUM | Human male | global_chr.s3d | globalhum_chr.s3d |
| IKF | Iksar female | global4_chr.s3d | globalikf_chr.s3d |
| IKM | Iksar male | global4_chr.s3d | globalikm_chr.s3d |
| KEF | Vah Shir female | global7_chr.s3d | globalkef_chr.s3d |
| KEM | Vah Shir male | global7_chr.s3d | globalkem_chr.s3d |
| OGF | Ogre female | global_chr.s3d | globalogf_chr.s3d |
| OGM | Ogre male | global_chr.s3d | globalogm_chr.s3d |
| SKE | Skeleton | global_chr.s3d | global6_chr.s3d |
| TRF | Troll female | global_chr.s3d | globaltrf_chr.s3d |
| TRM | Troll male | global_chr.s3d | globaltrm_chr.s3d |
| WOE |  | global_chr.s3d | global6_chr.s3d |

## Characters & NPCs — global model index

| code | name | archives | status |
|---|---|---|---|
| AEL | Air Elemental | global5_chr.s3d | new-only |
| ALL | Alligator | global6_chr.s3d | new-only |
| BAF | Barbarian female | global4_chr.s3d, global_chr.s3d, globalbaf_chr.s3d | REPLACED |
| BAM | Barbarian male | global4_chr.s3d, global_chr.s3d, globalbam_chr.s3d | REPLACED |
| BEA | Bear | global2_chr.s3d | new-only |
| BOAT | Boat | global_chr.s3d | classic |
| BRI | Bristlebane | global2_chr.s3d | new-only |
| CAZ | Cazic-Thule | global2_chr.s3d | new-only |
| DAF | Dark Elf female | global_chr.s3d, globaldaf_chr.s3d | REPLACED |
| DAM | Dark Elf male | global_chr.s3d, globaldam_chr.s3d | REPLACED |
| DRK | Drake | global6_chr.s3d | new-only |
| DWF | Dwarf female | global_chr.s3d, globaldwf_chr.s3d | REPLACED |
| DWM | Dwarf male | global_chr.s3d, globaldwm_chr.s3d | REPLACED |
| EEL | Earth Elemental | global5_chr.s3d | new-only |
| ELE | Elemental | global_chr.s3d | classic |
| ELF | Wood Elf female | global_chr.s3d, globalelf_chr.s3d | REPLACED |
| ELM | Wood Elf male | global_chr.s3d, globalelm_chr.s3d | REPLACED |
| ERF | Erudite female | global_chr.s3d, globalerf_chr.s3d | REPLACED |
| ERM | Erudite male | global_chr.s3d, globalerm_chr.s3d | REPLACED |
| ERO | Erollisi Marr | global2_chr.s3d | new-only |
| EYE | Eye of Zomm | global_chr.s3d | classic |
| FEL | Fire Elemental | global5_chr.s3d | new-only |
| FRF | Froglok female | globalpcfroglok_chr.s3d | new-only |
| FRG |  | globalfroglok_chr.s3d | new-only |
| FRM | Froglok male | globalpcfroglok_chr.s3d | new-only |
| FRO |  | globalfroglok_chr.s3d | new-only |
| GNF | Gnome female | global_chr.s3d, globalgnf_chr.s3d | REPLACED |
| GNM | Gnome male | global_chr.s3d, globalgnm_chr.s3d | REPLACED |
| HAF | Half Elf female | global_chr.s3d, globalhaf_chr.s3d | REPLACED |
| HAM | Half Elf male | global_chr.s3d, globalham_chr.s3d | REPLACED |
| HIF | High Elf female | global_chr.s3d, globalhif_chr.s3d | REPLACED |
| HIM | High Elf male | global_chr.s3d, globalhim_chr.s3d | REPLACED |
| HOF | Halfling female | global_chr.s3d, globalhof_chr.s3d | REPLACED |
| HOM | Halfling male | global_chr.s3d, globalhom_chr.s3d | REPLACED |
| HSM |  | global5_chr.s3d | new-only |
| HUF | Human female | global_chr.s3d, globalhuf_chr.s3d | REPLACED |
| HUM | Human male | global_chr.s3d, globalhum_chr.s3d | REPLACED |
| IKF | Iksar female | global4_chr.s3d, globalikf_chr.s3d | REPLACED |
| IKM | Iksar male | global4_chr.s3d, globalikm_chr.s3d | REPLACED |
| IKS | Iksar Skeleton | global4_chr.s3d | new-only |
| IMP | Imp | global2_chr.s3d | new-only |
| INN | Innoruuk | global2_chr.s3d | new-only |
| IVM |  | global_chr.s3d | classic |
| KEF | Vah Shir female | global7_chr.s3d, globalkef_chr.s3d | REPLACED |
| KEM | Vah Shir male | global7_chr.s3d, globalkem_chr.s3d | REPLACED |
| OGF | Ogre female | global_chr.s3d, globalogf_chr.s3d | REPLACED |
| OGM | Ogre male | global_chr.s3d, globalogm_chr.s3d | REPLACED |
| RAL |  | global2_chr.s3d | new-only |
| SCA |  | global2_chr.s3d | new-only |
| SKE | Skeleton | global6_chr.s3d, global_chr.s3d | REPLACED |
| SOL |  | global2_chr.s3d | new-only |
| SPE | Spectre | global4_chr.s3d | new-only |
| TIG | Tiger | global6_chr.s3d | new-only |
| TPN |  | global6_chr.s3d | new-only |
| TRF | Troll female | global_chr.s3d, globaltrf_chr.s3d | REPLACED |
| TRI |  | global2_chr.s3d | new-only |
| TRM | Troll male | global_chr.s3d, globaltrm_chr.s3d | REPLACED |
| TUN |  | global2_chr.s3d | new-only |
| WEL |  | global5_chr.s3d | new-only |
| WER | Werewolf | global_chr.s3d | classic |
| WOE |  | global6_chr.s3d, global_chr.s3d | REPLACED |
| WOF |  | global6_chr.s3d | new-only |
| WOL | Wolf | global6_chr.s3d | new-only |

## NPC model-code legend

Every distinct NPC model code across the global and zone archives (448 total; 86 named). Names are a best-effort map — player races are exact, deities are from bonzz.com/eqzoo, common creatures from EQ code convention; blanks are codes not confidently identified (the code→creature bridge is client-side, not in the DB). `#zones` = how many zone archives bundle the model.

| code | name | #zones | global? |
|---|---|---|---|
| AAM |  | 5 |  |
| ABH |  | 2 |  |
| AEL | Air Elemental | 1 | global |
| AKM |  | 8 |  |
| AKN |  | 4 |  |
| ALL | Alligator | 22 | global |
| AMP |  | 1 |  |
| ARM | Armadillo | 5 |  |
| AVI | Aviak | 11 |  |
| BAC |  | 10 |  |
| BAF | Barbarian female | 0 | global |
| BAM | Barbarian male | 0 | global |
| BAT | Bat | 31 |  |
| BEA | Bear | 21 | global |
| BEH |  | 8 |  |
| BET | Beetle | 29 |  |
| BGB |  | 1 |  |
| BGG |  | 4 |  |
| BGM |  | 19 |  |
| BIX | Bixie | 8 |  |
| BKN |  | 4 |  |
| BOAT | Boat | 0 | global |
| BON |  | 2 |  |
| BOX |  | 2 |  |
| BRC |  | 1 |  |
| BRF |  | 10 |  |
| BRI | Bristlebane | 0 | global |
| BRL |  | 2 |  |
| BRM |  | 9 |  |
| BRN |  | 6 |  |
| BRV |  | 5 |  |
| BTM |  | 2 |  |
| BTP |  | 1 |  |
| BTX |  | 3 |  |
| BUB |  | 2 |  |
| BUU |  | 2 |  |
| BVK |  | 1 |  |
| BWD |  | 7 |  |
| CAZ | Cazic-Thule | 46 | global |
| CCD |  | 4 |  |
| CEN | Centaur | 6 |  |
| CL |  | 1 |  |
| CLB |  | 2 |  |
| CLF |  | 4 |  |
| CLG |  | 1 |  |
| CLM |  | 6 |  |
| COC |  | 3 |  |
| COF |  | 12 |  |
| COK |  | 2 |  |
| COM |  | 15 |  |
| CPF |  | 6 |  |
| CPM |  | 6 |  |
| CPT |  | 1 |  |
| CRB |  | 3 |  |
| CRO |  | 2 |  |
| CST |  | 2 |  |
| CUB | Bear Cub | 14 |  |
| CWB |  | 1 |  |
| DAF | Dark Elf female | 0 | global |
| DAM | Dark Elf male | 0 | global |
| DEN |  | 7 |  |
| DER |  | 15 |  |
| DEV |  | 5 |  |
| DIW |  | 8 |  |
| DJI | Djinn | 13 |  |
| DML |  | 3 |  |
| DPF |  | 1 |  |
| DPM |  | 1 |  |
| DR2 |  | 13 |  |
| DRA | Drake | 22 |  |
| DRF |  | 8 |  |
| DRI |  | 12 |  |
| DRK | Drake | 19 | global |
| DRM |  | 8 |  |
| DRU |  | 7 |  |
| DRV |  | 1 |  |
| DSB |  | 3 |  |
| DVM |  | 2 |  |
| DWF | Dwarf female | 0 | global |
| DWM | Dwarf male | 0 | global |
| EEL | Earth Elemental | 2 | global |
| EEY |  | 1 |  |
| EFC |  | 3 |  |
| EFE |  | 5 |  |
| EFR | Efreeti | 23 |  |
| EGM |  | 4 |  |
| ELE | Elemental | 1 | global |
| ELF | Wood Elf female | 0 | global |
| ELM | Wood Elf male | 0 | global |
| EMP |  | 3 |  |
| ENA |  | 6 |  |
| EPF |  | 1 |  |
| EPM |  | 1 |  |
| ERF | Erudite female | 0 | global |
| ERM | Erudite male | 0 | global |
| ERO | Erollisi Marr | 0 | global |
| EYE | Eye of Zomm | 0 | global |
| FAF |  | 9 |  |
| FBF |  | 2 |  |
| FBM |  | 1 |  |
| FDR |  | 7 |  |
| FEF |  | 2 |  |
| FEL | Fire Elemental | 3 | global |
| FEM |  | 5 |  |
| FEN | Fennin Ro | 1 |  |
| FGH |  | 1 |  |
| FGI |  | 6 |  |
| FIS | Fish | 39 |  |
| FMO |  | 2 |  |
| FMP |  | 5 |  |
| FMT |  | 2 |  |
| FPM |  | 8 |  |
| FRF | Froglok female | 0 | global |
| FRG |  | 18 | global |
| FRM | Froglok male | 0 | global |
| FRO |  | 17 | global |
| FRT |  | 9 |  |
| FSG |  | 11 |  |
| FSK | Froglok Skeleton | 1 |  |
| FUD |  | 1 |  |
| FUG |  | 8 |  |
| FUN |  | 11 |  |
| GAL |  | 7 |  |
| GAM |  | 4 |  |
| GAR | Gargoyle | 17 |  |
| GBL |  | 1 |  |
| GDM |  | 10 |  |
| GDR |  | 3 |  |
| GEF |  | 9 |  |
| GEM |  | 8 |  |
| GFF |  | 2 |  |
| GFM |  | 2 |  |
| GGL |  | 4 |  |
| GHU | Ghoul | 22 |  |
| GIA | Giant | 15 |  |
| GLC |  | 1 |  |
| GLM |  | 1 |  |
| GMF |  | 7 |  |
| GMM |  | 7 |  |
| GMN |  | 7 |  |
| GNF | Gnome female | 0 | global |
| GNM | Gnome male | 0 | global |
| GNN | Gnoll | 19 |  |
| GOB | Goblin | 24 |  |
| GOJ |  | 1 |  |
| GOL | Golem | 39 |  |
| GOM |  | 7 |  |
| GOO |  | 14 |  |
| GOR |  | 13 |  |
| GPF |  | 1 |  |
| GPM |  | 1 |  |
| GRF |  | 2 |  |
| GRG | Gargoyle | 3 |  |
| GRI | Griffon | 11 |  |
| GRM |  | 5 |  |
| GSF |  | 5 |  |
| GSM |  | 4 |  |
| GSN |  | 7 |  |
| GSP |  | 1 |  |
| GTD |  | 4 |  |
| GVK |  | 1 |  |
| HAF | Half Elf female | 0 | global |
| HAG |  | 1 |  |
| HAM | Half Elf male | 0 | global |
| HAR | Harpy | 3 |  |
| HHM |  | 2 |  |
| HIF | High Elf female | 0 | global |
| HIM | High Elf male | 0 | global |
| HLF |  | 3 |  |
| HLM |  | 3 |  |
| HOF | Halfling female | 0 | global |
| HOM | Halfling male | 0 | global |
| HPF |  | 1 |  |
| HPM |  | 1 |  |
| HSF |  | 2 |  |
| HSM |  | 0 | global |
| HSN |  | 1 |  |
| HSS |  | 1 |  |
| HUF | Human female | 0 | global |
| HUM | Human male | 0 | global |
| IBR |  | 1 |  |
| ICF |  | 15 |  |
| ICM |  | 15 |  |
| ICN |  | 17 |  |
| ICY |  | 1 |  |
| IEC |  | 1 |  |
| IFC |  | 1 |  |
| IHU |  | 1 |  |
| IKF | Iksar female | 0 | global |
| IKG |  | 11 |  |
| IKH |  | 2 |  |
| IKM | Iksar male | 0 | global |
| IKS | Iksar Skeleton | 1 | global |
| ILA |  | 1 |  |
| ILB |  | 1 |  |
| IMP | Imp | 28 | global |
| INN | Innoruuk | 49 | global |
| ISB |  | 1 |  |
| ISC |  | 1 |  |
| ISE |  | 1 |  |
| IVM |  | 1 | global |
| IWF |  | 1 |  |
| IWH |  | 1 |  |
| IWM |  | 1 |  |
| IZF |  | 1 |  |
| IZM |  | 1 |  |
| JKR |  | 1 |  |
| JUB |  | 1 |  |
| KA |  | 2 |  |
| KAF |  | 3 |  |
| KAM |  | 4 |  |
| KAR | Karana | 2 |  |
| KED |  | 2 |  |
| KEF | Vah Shir female | 0 | global |
| KEM | Vah Shir male | 0 | global |
| KES |  | 9 |  |
| KGO |  | 14 |  |
| KHA |  | 4 |  |
| KOB | Kobold | 20 |  |
| KOP |  | 3 |  |
| KRK |  | 2 |  |
| LAUNCH |  | 6 |  |
| LAUNCHM |  | 7 |  |
| LCR |  | 7 |  |
| LEE |  | 7 |  |
| LEP |  | 3 |  |
| LGA |  | 1 |  |
| LGR |  | 1 |  |
| LIF |  | 7 |  |
| LIM |  | 13 |  |
| LIZ |  | 8 |  |
| LMF |  | 1 |  |
| LMM |  | 1 |  |
| LUG |  | 1 |  |
| LUJ |  | 3 |  |
| LYC |  | 5 |  |
| MAL |  | 3 |  |
| MAM |  | 6 |  |
| MAR |  | 1 |  |
| MEP |  | 10 |  |
| MER |  | 6 |  |
| MIM |  | 19 |  |
| MIN | Minotaur | 14 |  |
| MMF |  | 1 |  |
| MMM |  | 1 |  |
| MMV |  | 1 |  |
| MMY |  | 1 |  |
| MOI |  | 3 |  |
| MOS |  | 7 |  |
| MTC |  | 3 |  |
| MUH |  | 12 |  |
| NBT |  | 1 |  |
| NET |  | 5 |  |
| NGM |  | 6 |  |
| NIN |  | 1 |  |
| NMG |  | 3 |  |
| NMH |  | 5 |  |
| NMP |  | 1 |  |
| NMW |  | 2 |  |
| NPT |  | 2 |  |
| NYD |  | 2 |  |
| NYM |  | 3 |  |
| OGF | Ogre female | 0 | global |
| OGM | Ogre male | 0 | global |
| OKF |  | 3 |  |
| OKM |  | 3 |  |
| OPF |  | 1 |  |
| OPM |  | 1 |  |
| ORC | Orc | 40 |  |
| OTM |  | 4 |  |
| OWB |  | 4 |  |
| PAF |  | 8 |  |
| PBR |  | 2 |  |
| PEG | Pegasus | 5 |  |
| PHX |  | 4 |  |
| PIF |  | 12 |  |
| PIR |  | 32 |  |
| PRE |  | 8 |  |
| PRI |  | 5 |  |
| PUM | Puma | 21 |  |
| PUS |  | 3 |  |
| QCF |  | 26 |  |
| QCM |  | 25 |  |
| QZT |  | 1 |  |
| RAK |  | 2 |  |
| RAL |  | 1 | global |
| RAP |  | 8 |  |
| RAT | Rat | 42 |  |
| RAZ |  | 2 |  |
| REA |  | 14 |  |
| REM |  | 11 |  |
| RGM |  | 10 |  |
| RHI |  | 3 |  |
| RHP |  | 9 |  |
| RIF |  | 3 |  |
| RIM |  | 3 |  |
| RNB |  | 9 |  |
| ROE |  | 1 |  |
| ROM |  | 1 |  |
| RTH |  | 2 |  |
| RZM |  | 3 |  |
| SAR |  | 2 |  |
| SBU |  | 5 |  |
| SCA |  | 4 | global |
| SCE |  | 2 |  |
| SCH |  | 2 |  |
| SCR |  | 7 |  |
| SDC |  | 1 |  |
| SDE |  | 3 |  |
| SDF |  | 13 |  |
| SDM |  | 9 |  |
| SEA |  | 4 |  |
| SED |  | 13 |  |
| SEF |  | 1 |  |
| SEM |  | 1 |  |
| SER |  | 1 |  |
| SGO |  | 5 |  |
| SGR |  | 14 |  |
| SHA |  | 11 |  |
| SHF |  | 3 |  |
| SHIP |  | 9 |  |
| SHM |  | 3 |  |
| SHN |  | 7 |  |
| SHP |  | 1 |  |
| SHR |  | 7 |  |
| SIF |  | 1 |  |
| SIM |  | 19 |  |
| SIR |  | 4 |  |
| SKB |  | 4 |  |
| SKE | Skeleton | 2 | global |
| SKN |  | 7 |  |
| SKR |  | 2 |  |
| SKT | Skeleton | 1 |  |
| SKU |  | 3 |  |
| SLG |  | 4 |  |
| SMA |  | 4 |  |
| SNA | Snake | 34 |  |
| SNN |  | 5 |  |
| SOL |  | 0 | global |
| SOW |  | 6 |  |
| SPB |  | 1 |  |
| SPC |  | 1 |  |
| SPD |  | 11 |  |
| SPE | Spectre | 10 | global |
| SPH | Sphinx | 6 |  |
| SPI | Spider | 44 |  |
| SPL |  | 4 |  |
| SPR |  | 1 |  |
| SRG |  | 2 |  |
| SRO |  | 1 |  |
| SRV |  | 1 |  |
| SRW |  | 13 |  |
| SSA |  | 2 |  |
| SSK |  | 9 |  |
| SSN |  | 14 |  |
| STA |  | 4 |  |
| STC |  | 6 |  |
| STF |  | 4 |  |
| STG |  | 10 |  |
| STM |  | 3 |  |
| STR |  | 5 |  |
| STU |  | 6 |  |
| SUC |  | 3 |  |
| SVO |  | 2 |  |
| SWO |  | 8 |  |
| SYN |  | 1 |  |
| TAC |  | 1 |  |
| TAZ |  | 2 |  |
| TBF |  | 1 |  |
| TBL |  | 2 |  |
| TBM |  | 1 |  |
| TBU |  | 1 |  |
| TEF |  | 1 |  |
| TEG |  | 12 |  |
| TEM |  | 1 |  |
| TEN |  | 10 |  |
| TGL |  | 1 |  |
| THO |  | 8 |  |
| TIG | Tiger | 12 | global |
| TIN |  | 1 |  |
| TMB |  | 1 |  |
| TMR |  | 1 |  |
| TMT |  | 3 |  |
| TNF |  | 1 |  |
| TNM |  | 1 |  |
| TOT |  | 2 |  |
| TPB |  | 1 |  |
| TPN |  | 0 | global |
| TPO |  | 1 |  |
| TRE |  | 9 |  |
| TRF | Troll female | 0 | global |
| TRI |  | 0 | global |
| TRK |  | 8 |  |
| TRM | Troll male | 0 | global |
| TRN |  | 5 |  |
| TRT |  | 3 |  |
| TRW |  | 4 |  |
| TSM |  | 1 |  |
| TTB |  | 1 |  |
| TUN |  | 0 | global |
| TVP |  | 1 |  |
| TWF |  | 1 |  |
| TZF |  | 1 |  |
| TZM |  | 1 |  |
| UDF |  | 5 |  |
| UDK |  | 5 |  |
| UNB |  | 11 |  |
| UNI |  | 14 |  |
| VAC |  | 15 |  |
| VAL |  | 1 |  |
| VAS |  | 2 |  |
| VAZ |  | 2 |  |
| VEG |  | 5 |  |
| VEK |  | 1 |  |
| VOL |  | 3 |  |
| VPM |  | 5 |  |
| VRF |  | 2 |  |
| VRM |  | 6 |  |
| VSF |  | 10 |  |
| VSM |  | 10 |  |
| VST |  | 2 |  |
| WAL |  | 4 |  |
| WAS | Wasp | 6 |  |
| WBU |  | 2 |  |
| WEL |  | 2 | global |
| WER | Werewolf | 0 | global |
| WET |  | 8 |  |
| WIL | Will-o-Wisp | 25 |  |
| WLM |  | 8 |  |
| WMP |  | 2 |  |
| WOE |  | 0 | global |
| WOF |  | 5 | global |
| WOL | Wolf | 26 | global |
| WRB |  | 2 |  |
| WRF |  | 3 |  |
| WRU |  | 2 |  |
| WRW |  | 2 |  |
| WUF |  | 1 |  |
| WUR |  | 8 |  |
| WYV |  | 10 |  |
| XAL |  | 2 |  |
| XEG |  | 1 |  |
| YAK |  | 3 |  |
| YET |  | 5 |  |
| ZBC |  | 1 |  |
| ZEL |  | 5 |  |
| ZOF |  | 21 |  |
| ZOM | Zombie | 33 |  |

## Characters & NPCs — per-model `.eqg` archives

Later-expansion creature/character models shipped as `.eqg`. A code that also appears in an `.s3d` archive above is an `.eqg`-over-`.s3d` replacement.

| archive | models (.mod/.mds) | also in .s3d (replaces) |
|---|---|---|
| B09.eqg | B09, COL_B09 | — |
| B10.eqg | B10, COL_B10 | — |
| aam.eqg | AAM | — |
| ahf.eqg | AHF | — |
| ahm.eqg | AHM | — |
| aie.eqg | AIE | — |
| ala.eqg | ALA | — |
| alg.eqg | ALG | — |
| amy.eqg | AMY | — |
| ans.eqg | ANS | — |
| apx.eqg | APX | — |
| aro.eqg | ARO | — |
| asm.eqg | ASM | — |
| avk.eqg | AVK | — |
| axa.eqg | AXA | — |
| b04.eqg | B04, COL_B04 | — |
| b05.eqg | B05, COL_B05 | — |
| b06.eqg | B06, COL_B06 | — |
| bal.eqg | BAL | — |
| bas.eqg | BAS, BAS_LOD1, BAS_LOD2 | — |
| bat.eqg | BAT | — |
| bdr.eqg | BDR | — |
| bel.eqg | BEL | — |
| bfc.eqg | BFC | — |
| bfr.eqg | BFR | — |
| bkd.eqg | BKD | — |
| blv.eqg | BLV | — |
| bnf.eqg | BNF | — |
| bnm.eqg | BNM | — |
| bnr.eqg | OBP_BNR_BLUE00, OBP_BNR_BROWN00, OBP_BNR_GREEN00, OBP_BNR_GREY00, OBP_BNR_LTBLUE00, OBP_BNR_ORANGE00, OBP_BNR_PURPLE00, OBP_BNR_RED00, OBP_BNR_YELLOW00 | — |
| bnx.eqg | BNX | — |
| bny.eqg | BNY | — |
| bre.eqg | BRE | — |
| brx.eqg | BRX | — |
| bse.eqg | BSE | — |
| bsg.eqg | BSG | — |
| btl.eqg | BTL | — |
| btn.eqg | BTN | — |
| bts.eqg | BTS | — |
| bur.eqg | BUR | — |
| bxi.eqg | BXI | — |
| cak.eqg | CAK, IT11373, IT11374 | — |
| cam.eqg | CAM | — |
| cdf.eqg | CDF | — |
| cdm.eqg | CDM | — |
| cdr.eqg | CDR | — |
| chm.eqg | CHM | — |
| christmaselfhat.eqg | GENERIC_ROBE_RED_IKSAR_MALE, IT29000, IT29001, IT29002, IT29003, IT29004, IT29005, IT29006, IT29007, IT29008, IT29009, IT29010 … | — |
| cht.eqg | CHT | — |
| clq.eqg | CLQ | — |
| cls.eqg | CLS | — |
| clv.eqg | CLV | — |
| clw.eqg | CLW | — |
| cnp.eqg | CNP | — |
| cnt.eqg | CNT | — |
| crh.eqg | CRH, CRH_LOD1, CRH_LOD2 | — |
| crl.eqg | CRL, CRL_LOD1, CRL_LOD2 | — |
| crs.eqg | CRS | — |
| cry.eqg | CRY | — |
| cse.eqg | CSE | — |
| cst.eqg | CST | — |
| cth.eqg | CTH | — |
| cwg.eqg | CWG | — |
| cwr.eqg | CWR | — |
| dbp.eqg | DBP | — |
| dbx.eqg | DBX | — |
| dcf.eqg | DCF, DCF_LOD1, DCF_LOD2 | — |
| dcm.eqg | DCM, DCM_LOD1, DCM_LOD2 | — |
| ddm.eqg | DDM | — |
| ddv.eqg | DDV | — |
| dest_ballista.eqg | DEST_BALLISTA_A01, DEST_BALLISTA_A02, DEST_BALLISTA_A03, DEST_BALLISTA_A04, DEST_BALLISTA_M00, DEST_BALLISTA_M01, DEST_BALLISTA_M02, DEST_BALLISTA_M03, DEST_BALLISTA_M04, OBJ_BALLISTA, OBJ_BALLISTA1, OBJ_BALLISTA2 … | — |
| dest_catapult.eqg | DEST_CATAPULT_A01, DEST_CATAPULT_A02, DEST_CATAPULT_A03, DEST_CATAPULT_A04, DEST_CATAPULT_M00, DEST_CATAPULT_M01, DEST_CATAPULT_M02, DEST_CATAPULT_M03, DEST_CATAPULT_M04, OBJ_CATAPULTA, OBJ_CATAPULTA1, OBJ_CATAPULTA2 … | — |
| dest_cgg.eqg | DEST_CGG, DEST_CGG_A01, DEST_CGG_A02, DEST_CGG_A03, DEST_CGG_A04, DEST_CGG_M00, DEST_CGG_M01, DEST_CGG_M02, DEST_CGG_M03, DEST_CGG_M04 | — |
| dest_cgi.eqg | DEST_CGI, DEST_CGI_A01, DEST_CGI_A02, DEST_CGI_A03, DEST_CGI_A04, DEST_CGI_M00, DEST_CGI_M01, DEST_CGI_M02, DEST_CGI_M03, DEST_CGI_M04 | — |
| dest_cgir.eqg | DEST_CGIR, DEST_CGIR_A01, DEST_CGIR_A02, DEST_CGIR_A03, DEST_CGIR_A04, DEST_CGIR_M00, DEST_CGIR_M01, DEST_CGIR_M02, DEST_CGIR_M03, DEST_CGIR_M04 | — |
| dest_cgw.eqg | DEST_CGW, DEST_CGW_A01, DEST_CGW_A02, DEST_CGW_A03, DEST_CGW_A04, DEST_CGW_M00, DEST_CGW_M01, DEST_CGW_M02, DEST_CGW_M03, DEST_CGW_M04 | — |
| dest_dev_fortcolumn.eqg | DEST_DEV_FORTCOLUMN_A01, DEST_DEV_FORTCOLUMN_A02, DEST_DEV_FORTCOLUMN_A03, DEST_DEV_FORTCOLUMN_A04, DEST_DEV_FORTCOLUMN_M00, DEST_DEV_FORTCOLUMN_M01, DEST_DEV_FORTCOLUMN_M02, DEST_DEV_FORTCOLUMN_M03, DEST_DEV_FORTCOLUMN_M04, OBJ_FORTWALL_TALL_CONNECTOR_DESTRUCT_00_, OBJ_FORTWALL_TALL_CONNECTOR_DESTRUCT_00_1, OBJ_FORTWALL_TALL_CONNECTOR_DESTRUCT_00_2 … | — |
| dest_dev_fortdoor.eqg | DEST_DEV_FORTDOOR_A01, DEST_DEV_FORTDOOR_A02, DEST_DEV_FORTDOOR_A03, DEST_DEV_FORTDOOR_A04, DEST_DEV_FORTDOOR_M00, DEST_DEV_FORTDOOR_M01, DEST_DEV_FORTDOOR_M02, DEST_DEV_FORTDOOR_M03, DEST_DEV_FORTDOOR_M04, OBJ_RAGE_DOORA, OBJ_RAGE_DOORA1, OBJ_RAGE_DOORACOL | — |
| dest_dev_fortwall.eqg | DEST_DEV_FORTWALL_A01, DEST_DEV_FORTWALL_A02, DEST_DEV_FORTWALL_A03, DEST_DEV_FORTWALL_A04, DEST_DEV_FORTWALL_M00, DEST_DEV_FORTWALL_M01, DEST_DEV_FORTWALL_M02, DEST_DEV_FORTWALL_M03, DEST_DEV_FORTWALL_M04, OBJ_FORTWALL_TALL_DESTRUCT_00_, OBJ_FORTWALL_TALL_DESTRUCT_00_1, OBJ_FORTWALL_TALL_DESTRUCT_00_2 … | — |
| dest_dev_tent1.eqg | DEST_DEV_TENT1_A01, DEST_DEV_TENT1_A02, DEST_DEV_TENT1_A03, DEST_DEV_TENT1_A04, DEST_DEV_TENT1_M00, DEST_DEV_TENT1_M01, DEST_DEV_TENT1_M02, DEST_DEV_TENT1_M03, DEST_DEV_TENT1_M04, OBJ_TENTCCOL, OBJ_TENTC_DESTRUCT_, OBJ_TENTC_DESTRUCT_1 … | — |
| dest_dev_tent2.eqg | DEST_DEV_TENT2_A01, DEST_DEV_TENT2_A02, DEST_DEV_TENT2_A03, DEST_DEV_TENT2_A04, DEST_DEV_TENT2_M00, DEST_DEV_TENT2_M01, DEST_DEV_TENT2_M02, DEST_DEV_TENT2_M03, DEST_DEV_TENT2_M04 | — |
| dest_dev_tent3.eqg | DEST_DEV_TENT3_A01, DEST_DEV_TENT3_A02, DEST_DEV_TENT3_A03, DEST_DEV_TENT3_A04, DEST_DEV_TENT3_M00, DEST_DEV_TENT3_M01, DEST_DEV_TENT3_M02, DEST_DEV_TENT3_M03, DEST_DEV_TENT3_M04, OBJ_TENTACOL, OBJ_TENTA_DESTRUCT_, OBJ_TENTA_DESTRUCT_1 … | — |
| dest_elddar_door.eqg | DEST_ELDDAR_DOOR_A01, DEST_ELDDAR_DOOR_A02, DEST_ELDDAR_DOOR_A03, DEST_ELDDAR_DOOR_A04, DEST_ELDDAR_DOOR_M00, DEST_ELDDAR_DOOR_M01, DEST_ELDDAR_DOOR_M02, DEST_ELDDAR_DOOR_M03, DEST_ELDDAR_DOOR_M04, OBJ_CITYDOOR, OBJ_CITYDOOR1, OBJ_CITYDOOR2 … | — |
| dest_frst_f.eqg | DEST_FRST_F, DEST_FRST_F_A01, DEST_FRST_F_A02, DEST_FRST_F_A03, DEST_FRST_F_A04, DEST_FRST_F_M00, DEST_FRST_F_M01, DEST_FRST_F_M02, DEST_FRST_F_M03, DEST_FRST_F_M04 | — |
| dest_frst_w1.eqg | DEST_FRST_W1, DEST_FRST_W1_A01, DEST_FRST_W1_A02, DEST_FRST_W1_A03, DEST_FRST_W1_A04, DEST_FRST_W1_M00, DEST_FRST_W1_M01, DEST_FRST_W1_M02, DEST_FRST_W1_M03, DEST_FRST_W1_M04 | — |
| dest_frst_w2.eqg | DEST_FRST_W2, DEST_FRST_W2_A01, DEST_FRST_W2_A02, DEST_FRST_W2_A03, DEST_FRST_W2_A04, DEST_FRST_W2_M00, DEST_FRST_W2_M01, DEST_FRST_W2_M02, DEST_FRST_W2_M03, DEST_FRST_W2_M04 | — |
| dest_frst_w3.eqg | DEST_FRST_W3, DEST_FRST_W3_A01, DEST_FRST_W3_A02, DEST_FRST_W3_A03, DEST_FRST_W3_A04, DEST_FRST_W3_M00, DEST_FRST_W3_M01, DEST_FRST_W3_M02, DEST_FRST_W3_M03, DEST_FRST_W3_M04 | — |
| dest_gpt.eqg | DEST_GPT, DEST_GPT_A01, DEST_GPT_A02, DEST_GPT_A03, DEST_GPT_A04, DEST_GPT_M00, DEST_GPT_M01, DEST_GPT_M02, DEST_GPT_M03, DEST_GPT_M04 | — |
| dest_gpt_r.eqg | DEST_GPT_R, DEST_GPT_R_A01, DEST_GPT_R_A02, DEST_GPT_R_A03, DEST_GPT_R_A04, DEST_GPT_R_M00, DEST_GPT_R_M01, DEST_GPT_R_M02, DEST_GPT_R_M03, DEST_GPT_R_M04 | — |
| dest_mound.eqg | DEST_MOUND, DEST_MOUND_A01, DEST_MOUND_A02, DEST_MOUND_A03, DEST_MOUND_M00, DEST_MOUND_M01, DEST_MOUND_M02, DEST_MOUND_M03, DEST_MOUND_M04 | — |
| dest_pdm.eqg | DEST_PDM, DEST_PDM_A01, DEST_PDM_A02, DEST_PDM_A03, DEST_PDM_A04, DEST_PDM_M00, DEST_PDM_M01, DEST_PDM_M02, DEST_PDM_M03, DEST_PDM_M04 | — |
| dest_rnt_a.eqg | DEST_RNT_A, DEST_RNT_A_A01, DEST_RNT_A_A02, DEST_RNT_A_A03, DEST_RNT_A_A04, DEST_RNT_A_M00, DEST_RNT_A_M01, DEST_RNT_A_M02, DEST_RNT_A_M03, DEST_RNT_A_M04 | — |
| dest_rnt_b.eqg | DEST_RNT_B, DEST_RNT_B_A01, DEST_RNT_B_A02, DEST_RNT_B_A03, DEST_RNT_B_A04, DEST_RNT_B_M00, DEST_RNT_B_M01, DEST_RNT_B_M02, DEST_RNT_B_M03, DEST_RNT_B_M04 | — |
| dest_rnt_c.eqg | DEST_RNT_C, DEST_RNT_C_A01, DEST_RNT_C_A02, DEST_RNT_C_A03, DEST_RNT_C_A04, DEST_RNT_C_M00, DEST_RNT_C_M01, DEST_RNT_C_M02, DEST_RNT_C_M03, DEST_RNT_C_M04 | — |
| dest_sphere_shield.eqg | DEST_SPHERE_SHIELD_A01, DEST_SPHERE_SHIELD_A02, DEST_SPHERE_SHIELD_A03, DEST_SPHERE_SHIELD_A04, DEST_SPHERE_SHIELD_M00, DEST_SPHERE_SHIELD_M01, DEST_SPHERE_SHIELD_M02, DEST_SPHERE_SHIELD_M03, DEST_SPHERE_SHIELD_M04, OBJ_ROCK, OBJ_ROCK1, OBJ_ROCK2 … | — |
| dest_tak_orctent.eqg | DEST_TAK_ORCTENT_A01, DEST_TAK_ORCTENT_A02, DEST_TAK_ORCTENT_A03, DEST_TAK_ORCTENT_A04, DEST_TAK_ORCTENT_M00, DEST_TAK_ORCTENT_M01, DEST_TAK_ORCTENT_M02, DEST_TAK_ORCTENT_M03, DEST_TAK_ORCTENT_M04, OBJ_TENTACOL, OBJ_TENT_A_, OBJ_TENT_A_1 … | — |
| dest_tak_rootwall.eqg | DEST_TAK_ROOTWALL_A01, DEST_TAK_ROOTWALL_A02, DEST_TAK_ROOTWALL_A03, DEST_TAK_ROOTWALL_A04, DEST_TAK_ROOTWALL_M00, DEST_TAK_ROOTWALL_M01, DEST_TAK_ROOTWALL_M02, DEST_TAK_ROOTWALL_M03, DEST_TAK_ROOTWALL_M04, OBJ_DOORSWITCH, OBJ_DOORSWITCH1, OBJ_DOORSWITCH2 … | — |
| dest_tak_totem.eqg | DEST_TAK_TOTEM_A01, DEST_TAK_TOTEM_A02, DEST_TAK_TOTEM_A03, DEST_TAK_TOTEM_A04, DEST_TAK_TOTEM_M00, DEST_TAK_TOTEM_M01, DEST_TAK_TOTEM_M02, DEST_TAK_TOTEM_M03, DEST_TAK_TOTEM_M04, OBJ_TAKISHTOTEM, OBJ_TAKISHTOTEM1, OBJ_TAKISHTOTEM2 … | — |
| dest_theatera_bell.eqg | DEST_THEATERA_BELL_A01, DEST_THEATERA_BELL_A02, DEST_THEATERA_BELL_A03, DEST_THEATERA_BELL_A04, DEST_THEATERA_BELL_M00, DEST_THEATERA_BELL_M01, DEST_THEATERA_BELL_M02, DEST_THEATERA_BELL_M03, DEST_THEATERA_BELL_M04, OBJ_MAINBELL, OBJ_MAINBELL1, OBJ_MAINBELL2 … | — |
| dest_tnt_e.eqg | DEST_TNT_E, DEST_TNT_E_A01, DEST_TNT_E_A02, DEST_TNT_E_A03, DEST_TNT_E_A04, DEST_TNT_E_M00, DEST_TNT_E_M01, DEST_TNT_E_M02, DEST_TNT_E_M03, DEST_TNT_E_M04 | — |
| dest_tnt_g.eqg | DEST_TNT_G, DEST_TNT_G_A01, DEST_TNT_G_A02, DEST_TNT_G_A03, DEST_TNT_G_A04, DEST_TNT_G_M00, DEST_TNT_G_M01, DEST_TNT_G_M02, DEST_TNT_G_M03, DEST_TNT_G_M04 | — |
| dest_tst.eqg | DEST_TST, DEST_TST_A01, DEST_TST_A02, DEST_TST_A03, DEST_TST_A04, DEST_TST_M00, DEST_TST_M01, DEST_TST_M02, DEST_TST_M03, DEST_TST_M04 | — |
| dest_tst_r.eqg | DEST_TST_R, DEST_TST_R_A01, DEST_TST_R_A02, DEST_TST_R_A03, DEST_TST_R_A04, DEST_TST_R_M00, DEST_TST_R_M01, DEST_TST_R_M02, DEST_TST_R_M03, DEST_TST_R_M04 | — |
| dest_wallofdead.eqg | DEST_WALLOFDEAD_A01, DEST_WALLOFDEAD_A02, DEST_WALLOFDEAD_A03, DEST_WALLOFDEAD_A04, DEST_WALLOFDEAD_M00, DEST_WALLOFDEAD_M01, DEST_WALLOFDEAD_M02, DEST_WALLOFDEAD_M03, DEST_WALLOFDEAD_M04, OBJ_DOORSWITCH, OBJ_DOORSWITCH1, OBJ_DOORSWITCH2 … | — |
| dke.eqg | DKE | — |
| dkf.eqg | DKF, DKF_00_00_LEGL_THGH, DKF_00_00_LEGR_THGH, DKF_00_00_ROOT_BODY, DKF_00_01_ARML_BCEP, DKF_00_01_ARML_CLAV, DKF_00_01_ARMR_BCEP, DKF_00_01_ARMR_CLAV, DKF_00_01_LEGL_THGH, DKF_00_01_LEGR_THGH, DKF_00_01_PELV, DKF_00_01_ROOT_BODY … | — |
| dkm.eqg | DKM, DKM_00_00_LEGL_THGH, DKM_00_00_LEGR_THGH, DKM_00_00_ROOT_BODY, DKM_00_01_ARML_BCEP, DKM_00_01_ARML_CLAV, DKM_00_01_ARMR_BCEP, DKM_00_01_ARMR_CLAV, DKM_00_01_LEGL_THGH, DKM_00_01_LEGR_THGH, DKM_00_01_PELV, DKM_00_01_ROOT_BODY … | — |
| dma.eqg | DMA | — |
| dodequip.eqg | IT10810, IT10811, IT10812, IT10813, IT10814, IT10815, IT10816, IT10817, IT10818, IT10819, IT10820, IT10821 … | — |
| donequip.eqg | IT10780, IT10781, IT10782, IT10783, IT10784, IT10785, IT10786, IT10787, IT10788, IT10789, IT10790, IT10791 … | — |
| dragonscale.eqg | OBJ_AKHUT_INT, OBJ_AKNON_HUT, OBJ_ARCHSMALLWIND, OBJ_BARREL_WOOD, OBJ_BED_GNM, OBJ_BED_GNM01, OBJ_BLSTAND, OBJ_BONES_A, OBJ_BONES_B, OBJ_BONES_C, OBJ_BONES_D, OBJ_BOXOFCELLS … | — |
| dragonscalea.eqg | OBJ_BONES_A, OBJ_BONES_B, OBJ_BONES_C, OBJ_BONES_D, OBJ_CEILINGFAN, OBJ_CEILINGFAN_COL, OBJ_CRYSGREENA, OBJ_CRYSTALBLACKA, OBJ_CRYSTALBLACKB, OBJ_CRYSTALBLUEA, OBJ_CRYSTALBLUEB, OBJ_CRYSTALGREENA … | — |
| drc.eqg | DRC, DRC_LOD1, DRC_LOD2 | — |
| dre.eqg | DRE | — |
| drg.eqg | DRG | — |
| drl.eqg | DRL | — |
| drp.eqg | DRP | — |
| drs.eqg | DRS | — |
| dsf.eqg | DSF | — |
| dsg.eqg | DSG | — |
| dv6.eqg | DV6 | — |
| dvl.eqg | DVL | — |
| dvs.eqg | DVS | — |
| dyn.eqg | DYN | — |
| eae.eqg | EAE | — |
| eef.eqg | EEF | — |
| eem.eqg | EEM | — |
| eru.eqg | ERU | — |
| eve.eqg | EVE | — |
| exo.eqg | EXO | — |
| feerrott2.eqg | FEERROTT_ENTRANCE_POF_OBJ_FEERROTT_ENT_POF_PORTAL.LIT, OBJ_DOOR_2_THULE, OBJ_EYE, OBJ_FEERROTT_2_CAZIC_THULE, OBJ_FEERROTT_2_CAZIC_THULE_LOD1, OBJ_FEERROTT_BBQ, OBJ_FEERROTT_BBQ_LOD1, OBJ_FEERROTT_BRIDGE, OBJ_FEERROTT_BRIDGE_LOD1, OBJ_FEERROTT_DEADTREE, OBJ_FEERROTT_DEADTREE_LOD1, OBJ_FEERROTT_DEADTREE_LOD2 … | — |
| fgp.eqg | FGP, FGP_LOD1, FGP_LOD2 | — |
| fgt.eqg | FGT | — |
| fie.eqg | FIE | — |
| fkn.eqg | FKN | — |
| flg.eqg | FLG | — |
| fng.eqg | FNG | — |
| fra.eqg | FRA | — |
| frd.eqg | FRD | — |
| freeporteast.eqg | OBJ_ARENADOORL, OBJ_ARENADOORR, OBJ_ARENASTEPS, OBJ_ARENA_EXTERIOR, OBJ_ARENA_EXTERIOR1, OBJ_ARENA_EXTERIOR2, OBJ_ARENA_INTARCH, OBJ_ARENA_INTARCH1, OBJ_ARENA_NEWPOST, OBJ_ARENA_PILLARHEX, OBJ_ARENA_PILLARSQR, OBJ_ARENA_PILLARSQR1 … | — |
| frostfellelfhat.eqg | GENERIC_ROBE_RED_IKSAR_MALE, IT29000, IT29001, IT29002, IT29003, IT29004, IT29005, IT29006, IT29007, IT29008, IT29009, IT29010 … | — |
| fry.eqg | FRY | — |
| furniture12.eqg | IT23349, IT23350, IT23351, IT23352, IT23353, IT23354, IT23355, IT23356, IT23357, IT23358, IT23359, IT23360 … | — |
| furniture_12days.eqg | I31, IT23629, IT23630, IT23631 | — |
| g05.eqg | G05 | — |
| gbm.eqg | GBM | — |
| gbn.eqg | GBN | — |
| gcb.eqg | GCB | — |
| gen.eqg | GEN | — |
| gfc.eqg | GFC | — |
| gfo.eqg | GFO | — |
| gfr.eqg | GFR | — |
| gfs.eqg | GFS | — |
| ggy.eqg | GGY, GGY_LOD1, GGY_LOD2 | — |
| gho.eqg | GHO | — |
| gig.eqg | GIG | — |
| glb.eqg | GLB | — |
| glm.eqg | GLM | — |
| globalgdb.eqg | G00, G01, G02, G03, G04 | — |
| gnd.eqg | GND | — |
| gnl.eqg | GNL | — |
| god.eqg | GOD | — |
| gr1.eqg | GR1 | — |
| gra.eqg | GRA | — |
| grd.eqg | GRD | — |
| grl.eqg | GRL | — |
| grn.eqg | GRN | — |
| gua.eqg | GUA | — |
| guardian.eqg | OBJ_BOXOFCELLS, OBJ_BRACING, OBJ_CEILINGFAN, OBJ_CEILINGFAN_COL, OBJ_CLOSEDBOX, OBJ_CNTRLPNL, OBJ_CNTRL_BOX, OBJ_CNTRL_BOX_COL, OBJ_CNTRL_PANEL, OBJ_CNTRL_PANEL_COL, OBJ_COMFYCHAIR, OBJ_CONVEYORSTEP … | — |
| gul.eqg | GUL | — |
| gum.eqg | GUM | — |
| gus.eqg | GUS | — |
| gya.eqg | GYA | — |
| gyo.eqg | GYO | — |
| gyrospireb.eqg | OBJ_BARRELL_OIL, OBJ_BASEMENT_FAN, OBJ_BED, OBJ_BLUEPRINT, OBJ_BLUEPRINT_COAL, OBJ_BOOKSHELF_A, OBJ_BORE_GEAR, OBJ_BOTTLEA, OBJ_BOTTLEB, OBJ_BOWLA, OBJ_BOXOFCELLS, OBJ_BULLSEYE … | — |
| gyrospirez.eqg | OBJ_BARRELL_OIL, OBJ_BASEMENT_FAN, OBJ_BED, OBJ_BLUEPRINT, OBJ_BLUEPRINT_COAL, OBJ_BORE, OBJ_BORE_CHAIR, OBJ_BORE_GEAR, OBJ_BOXOFCELLS, OBJ_BULLSEYE, OBJ_CANNON_PART_A, OBJ_CANNON_PART_B … | — |
| harbingers.eqg | BCN, BT_CAVEIN, BT_DOOR, OBJ_BEACONCRYSTAL01, OBJ_BEAM_149, OBJ_BEAM_221, OBJ_BEAM_226, OBJ_BEAM_227, OBJ_BEAM_228, OBJ_BEAM_229, OBJ_BEAM_557, OBJ_BEAM_583 … | — |
| hdl.eqg | HDL | — |
| hdv.eqg | HDV | — |
| hlg.eqg | HLG | — |
| hnf.eqg | HNF | — |
| hnm.eqg | HNM | — |
| hrp.eqg | HRP, HRP_LOD1, HRP_LOD2 | — |
| hrs.eqg | HRS | — |
| hyc.eqg | HYC | — |
| i00.eqg | I00 | — |
| i01.eqg | I01 | — |
| i02.eqg | I02 | — |
| i03.eqg | I03 | — |
| i04.eqg | I04 | — |
| i05.eqg | I05 | — |
| i06.eqg | I06 | — |
| i07.eqg | I07 | — |
| i08.eqg | I08 | — |
| i09.eqg | I09 | — |
| i10.eqg | I10 | — |
| i11.eqg | I11 | — |
| i12.eqg | I12 | — |
| i13.eqg | I13 | — |
| i14.eqg | I14 | — |
| i15.eqg | I15 | — |
| i16.eqg | I16 | — |
| i18.eqg | I18 | — |
| i19.eqg | I19 | — |
| i20.eqg | I20 | — |
| i21.eqg | I21 | — |
| i22.eqg | I22 | — |
| i23.eqg | I23, INV | — |
| i24.eqg | I24 | — |
| i25.eqg | I25 | — |
| i26.eqg | I26 | — |
| i27.eqg | I27 | — |
| i28.eqg | COL_I28, I28 | — |
| i29.eqg | I29 | — |
| i30.eqg | I30, IT12096 | — |
| i31.eqg | I31 | — |
| i32.eqg | I32 | — |
| icy.eqg | ICY | — |
| inv.eqg | INV | — |
| it12095.eqg | GNOME_WAVE, IT12095, STND_GNOME_WAVE | — |
| kbd.eqg | KBD, KBD_LOD1, KBD_LOD2 | — |
| kdg.eqg | KDG | — |
| kng.eqg | KNG | — |
| kor.eqg | KOR, KOR_LOD1 | — |
| krb.eqg | KRB, KRB_LOD1 | — |
| krf.eqg | KRF | — |
| krm.eqg | KRM | — |
| krn.eqg | KRN | — |
| ldr.eqg | LDR | — |
| lon07.eqg | IT11378, IT11379, IT11384, IT11385, IT11386, IT11387, IT11388, IT11389, IT11390, IT11391, IT11530, IT11531 | — |
| lsp.eqg | LSP, LSP_LOD1, LSP_LOD2 | — |
| lsq.eqg | LSQ, LSQ_LOD1, LSQ_LOD2 | — |
| lth.eqg | LTH | — |
| lu2.eqg | LU2 | — |
| lu3.eqg | LU3 | — |
| lu4.eqg | LU4 | — |
| luc.eqg | LUC | — |
| lvr.eqg | LVR | — |
| mansion.eqg | OBJ_AIRVENTA, OBJ_AIRVENTB, OBJ_ARROWGUAGE, OBJ_BALL, OBJ_BEAKER_A, OBJ_BLIMP, OBJ_BONES, OBJ_BOOKCASE, OBJ_BOOKCASE_MELDRATH, OBJ_BOTTLEA, OBJ_BOTTLEB, OBJ_BOWLA … | — |
| map.eqg | MAP | — |
| mbl.eqg | MBL | — |
| mbr.eqg | MBR | — |
| mbx.eqg | MBX | — |
| mch.eqg | MCH | — |
| mcl.eqg | MCL | — |
| mcp.eqg | MCP | — |
| mcs.eqg | MCS | — |
| mdr.eqg | MDR | — |
| mes.eqg | MES | — |
| mfr.eqg | MFR | — |
| mgl.eqg | MGL | — |
| mhb.eqg | MHB | — |
| mhy.eqg | MHY | — |
| mkg.eqg | MKG | — |
| mki.eqg | MKI | — |
| mnr.eqg | MNR, MNR_LOD1, MNR_LOD2 | — |
| mnt.eqg | MNT | — |
| mpg.eqg | MPG | — |
| mph.eqg | MPH | — |
| mpu.eqg | MPU, MPU_BAK | — |
| mrd.eqg | MRD | — |
| mrh.eqg | MRH | — |
| mrk.eqg | MRK | — |
| mrp.eqg | MRP | — |
| mrt.eqg | MRT | — |
| msd.eqg | MSD | — |
| msl.eqg | MSL, STND_BA_1_MSL | — |
| mso.eqg | MSO | — |
| mta.eqg | MTA | — |
| mth.eqg | MTH | — |
| mtl.eqg | MTL | — |
| mtp.eqg | MTP | — |
| mtr.eqg | MTR | — |
| mud.eqg | MUD | — |
| mur.eqg | MUR | — |
| mwi.eqg | MWI | — |
| mwm.eqg | MWM | — |
| mwo.eqg | MWO | — |
| mwr.eqg | MWR | — |
| myg.eqg | MYG | — |
| northro.eqg | OBJ_AQUA_GBLNTEMPLE, OBJ_AQUA_GBLNTEMPLE1, OBJ_AQUA_GBLNTEMPLE2, OBJ_AQUA_GBLNTEMPLE3, OBJ_BANNER, OBJ_BANNER1, OBJ_BHUT, OBJ_BHUT1, OBJ_BHUT2, OBJ_BHUT3, OBJ_BUTTE_QUAD, OBJ_BUTTE_SIMPLE … | — |
| ogm.eqg | OGM, OGM_00_00_ROOT_BODY | OGM |
| omensequip.eqg | IT10735, IT10736, IT10737, IT10738, IT10739, IT10740, IT10741, IT10742, IT10743, IT10744, IT10745, IT10746 … | — |
| onm.eqg | ONM | — |
| orb.eqg | ORB | — |
| ork.eqg | ORK | — |
| pg3.eqg | PG3 | — |
| pgs.eqg | PGS | — |
| pma.eqg | PMA | — |
| prt.eqg | PRT | — |
| psc.eqg | PSC | — |
| rat.eqg | RAT | — |
| rdg.eqg | RDG | — |
| rkp.eqg | RKP | — |
| rob.eqg | ROB | — |
| rpt.eqg | RPT | — |
| rtn.eqg | RTN, RTN_LOD1, RTN_LOD2 | — |
| sat.eqg | SAT | — |
| scc.eqg | SCC | — |
| sco.eqg | SCO | — |
| scu.eqg | SCU | — |
| sdr.eqg | SDR | — |
| sdv.eqg | SDV | — |
| seg.eqg | SEG | — |
| sey.eqg | SEY | — |
| shd.eqg | SHD | — |
| she.eqg | SHE | — |
| shi.eqg | COL_SHI, SHI | — |
| shipmvp.eqg | OBJ_BUNK_DBDECK, OBJ_BUNK_DBDECKSEC, OBJ_BUNK_DBDECKTRD, OBJ_CPT_CABIN, OBJ_CPT_OFFICE, OBJ_CRATE_BROKEN, OBJ_DECK_FRTH, OBJ_DECK_MAIN, OBJ_DECK_SCND, OBJ_DECK_TRD, OBJ_DOOR, OBJ_DOOR_PIRATE … | — |
| shippvu.eqg | OBJ_BED_BUNKBED, OBJ_BED_FANCY, OBJ_BENCH, OBJ_BOOKSHELF_A, OBJ_BOOKSHELF_B, OBJ_BOOKSHELF_C, OBJ_BRIDGE, OBJ_CHAIR_PLAIN, OBJ_CPT_CABIN, OBJ_CPT_OFFICE, OBJ_CRATE_BROKEN, OBJ_DECK_FRTH … | — |
| shl.eqg | SHL | — |
| shs.eqg | SHS | — |
| sin.eqg | SIN | — |
| ski.eqg | SKI | — |
| smd.eqg | SMD | — |
| snd.eqg | SND | — |
| snk.eqg | SNK, SNK_LOD1, SNK_LOD2 | — |
| sofequip.eqg | IT11128, IT11129, IT11130, IT11131, IT11132, IT11133, IT11134, IT11135, IT11138, IT11139, IT11140, IT11141 … | — |
| sok.eqg | SOK | — |
| sos.eqg | SOS | — |
| spq.eqg | SPQ, SPQ_LOD1, SPQ_LOD2 | — |
| spt.eqg | SPT | — |
| spx.eqg | SPX | — |
| srk.eqg | SRK | — |
| srn.eqg | SRN | — |
| steamfactory.eqg | OBJ_ADJUSTED_CRATEA, OBJ_ADJUSTED_CRATEB, OBJ_ADJUSTED_CRATEC, OBJ_ADJUSTED_CRATED, OBJ_ADJUSTED_CRATEE, OBJ_ADJUSTED_CRATEF, OBJ_ASSMBLYRM, OBJ_BIGPIPE, OBJ_BIGWHEEL, OBJ_BIG_FAN_COLL, OBJ_BLASTDOOR, OBJ_BOXOFCELLS … | — |
| steamfontmts.eqg | OBJ_AKHALL_INT, OBJ_AKHUTA_EXT, OBJ_AKHUTA_EXT01, OBJ_AKHUTA_INT, OBJ_AKHUTB_EXT, OBJ_AKHUTB_INT, OBJ_AKSUPPORT, OBJ_AK_ENTRANCE, OBJ_BED_GNM, OBJ_BIGWHEEL, OBJ_BLOCKER_FRAME, OBJ_CEILINGFAN … | — |
| stillmoonb.eqg | CAVEFACADE, ET_DOOR01, ET_DRBANNERA_01, ET_DRBANNERB_01, ET_GATE01, OBJ_ARCHWAY, OBJ_AXE, OBJ_BOW, OBJ_BRACER, OBJ_BRAZIER_, OBJ_CHESTPLATE_LG, OBJ_CHESTPLATE_SM … | — |
| stonesnake.eqg | OBJ_ARTHWALL, OBJ_ARTHX_MUDBARREL, OBJ_ARTHX_MUDBARRELOPN, OBJ_ARTHX_TORCH, OBJ_BARREL_LG, OBJ_BARREL_SM, OBJ_BEAMSUPPORTSA1A, OBJ_BEAMSUPPORTSA2A, OBJ_BEAMSUPPORTSA3A, OBJ_BEAMSUPPORTSA4A, OBJ_BEAMSUPPORTSA5A, OBJ_BEAMSUPPORTSA6A … | — |
| swc.eqg | SWC | — |
| swi.eqg | SWI | — |
| szk.eqg | SZK | — |
| t00.eqg | T00 | — |
| t01.eqg | T01 | — |
| t02.eqg | T02 | — |
| t03.eqg | T03 | — |
| t04.eqg | T04 | — |
| t05.eqg | T05 | — |
| t06.eqg | T06 | — |
| t07.eqg | T07 | — |
| t08.eqg | T08 | — |
| t09.eqg | T09 | — |
| t10.eqg | T10 | — |
| t11.eqg | T11 | — |
| t12.eqg | T12 | — |
| t13.eqg | T13 | — |
| tar.eqg | TAR | — |
| tbsequip.eqg | IT11085, IT11086, IT11087, IT11088, IT11089, IT11090, IT11091, IT11092, IT11093, IT11094, IT11095, IT11096 … | — |
| tel.eqg | TEL | — |
| tgm.eqg | TGM | — |
| tgo.eqg | TGO | — |
| thuledream.eqg | OBJ_BANNERA, OBJ_BANNERB, OBJ_BANNERC, OBJ_BELL_RINGER, OBJ_BELL_RINGER_BROKE, OBJ_BENCH, OBJ_BRAZIER_HOUSE, OBJ_CAZIC_THRONE, OBJ_CHAIR, OBJ_COFFIN, OBJ_COFFIN_TOP, OBJ_DOOR_LOWER … | — |
| tln.eqg | TLN | — |
| tnt.eqg | TNT | — |
| tpl.eqg | TPL | — |
| tra.eqg | TRA | — |
| trg.eqg | TRG | — |
| tse.eqg | TSE | — |
| undequip.eqg | IT11375, IT11376, IT11377, IT11402, IT11403, IT11404, IT11405, IT11406, IT11407, IT11408, IT11409, IT11410 … | — |
| unm.eqg | UNM | — |
| vaf.eqg | VAF | — |
| vam.eqg | VAM | — |
| vnm.eqg | VNM | — |
| wae.eqg | WAE | — |
| wallet07.eqg | IT11400, IT11401 | — |
| wallet08.eqg | IT11548, IT11549, IT11550, IT11551, IT11552, IT11553, IT11554, IT11555 | — |
| wallet37.eqg | IT12315, IT12316, IT12317, IT12318, IT12319, IT12320, IT12321, IT12322, IT12323, IT12324, IT12325, IT12326 … | — |
| wallet40.eqg | IT12338, IT12339, IT12340, IT12341, IT12342, IT12343, IT12344, IT12345, IT12346, IT12347 | — |
| wlf.eqg | WLF | — |
| wok.eqg | WOK | — |
| wor.eqg | WOR | — |
| wrm.eqg | WRM | — |
| wwf.eqg | WWF, WWF_LOD1, WWF_LOD2 | — |
| wyr.eqg | WYR | — |
| xef.eqg | XEF | — |
| xem.eqg | XEM | — |
| xhf.eqg | XHF | — |
| xhm.eqg | XHM | — |
| xim.eqg | XIM | — |
| zmf.eqg | ZMF | — |
| zmm.eqg | ZMM, ZMM_LOD1, ZMM_LOD2 | — |

## Zone NPC index

NPC model codes unique to each zone archive. A code also found in a newer global archive (e.g. `global6_chr.s3d` Luclin creatures) is marked ⚠ — its classic zone version is shadowed.

| zone | # | NPC codes |
|---|---|---|
| acrylia | 12 | BIX, GMF, GMM, GMN, KES, SGO, SNA, SPI, SRV, THO, VAC, WET |
| ael | 1 | AEL⚠ |
| airplane | 19 | AVI, BEH, BIX, DER, DJI, DRI, DRK⚠, EFR, FAF, FDR, GRG, GRI, HAR, IMP⚠, PEG, PIF, SPH, WAS, WIL |
| akanon | 18 | BEA⚠, CAZ⚠, CL, CLF, CLM, CUB, FIS, FUN, INN⚠, KOB, MIN, ORC, PIR, RAT, REA, SNA, SPI, TIG⚠ |
| akheva | 7 | AKM, AKN, SDF, SDM, SGR, TEG, VAC |
| arena | 6 | BTM, DRA, EFR, GOR, LIM, TIG⚠ |
| befallen | 19 | BAT, CAZ⚠, CUB, DEN, FPM, GAR, GHU, GOL, IMP⚠, INN⚠, MIM, MIN, ORC, RAT, REA, TEN, WIL, ZOF, ZOM |
| beholder | 18 | ARM, BAT, BEH, BET, BGM, CAZ⚠, DER, DRK⚠, GNN, GOB, GOL, IMP⚠, INN⚠, MIN, PUM, SNA, SPI, WIL |
| bgb | 1 | BGB |
| blackburrow | 13 | ALL⚠, BEA⚠, CAZ⚠, GNN, GOB, GOL, INN⚠, MIM, ORC, PIR, RAT, SNA, TEN |
| bon | 1 | BON |
| bothunder | 13 | BRV, BTP, DER, GGL, KAR, SCE, SKR, SSA, STA, STF, STG, STR, SVO |
| box | 1 | BOX |
| brl | 1 | BRL |
| brv | 1 | BRV |
| burningwood | 10 | BAT, DRF, DRM, FGI, GOR, KGO, SRW, SSK, WAS, WUR |
| butcher2 | 1 | LAUNCHM |
| butcher | 19 | ALL⚠, AVI, BAT, BET, BRF, BRM, CAZ⚠, DRK⚠, GOB, INN⚠, KAF, KAM, ORC, PRE, SHIP, SKU, SNA, SPI, UNI |
| bvk | 1 | BVK |
| bwd | 1 | BWD |
| cabeast | 14 | BAC, FIS, FRO⚠, ICF, ICM, ICN, IKG, IKH, LAUNCHM, LEE, MOS, SCR, SIM, SSN |
| cabwest | 15 | BAC, BAT, FRO⚠, ICF, ICM, ICN, IKG, KGO, LAUNCHM, LEE, MOS, RAT, SCR, SIM, SSN |
| cauldron | 17 | ALL⚠, AVI, BEA⚠, BRF, BRM, CAZ⚠, GDM, GHU, GOB, GOL, INN⚠, KED, ORC, PIR, PUM, RAT, SNA |
| cazicthule | 16 | ALL⚠, CAZ⚠, DER, FIS, GHU, GOL, GOR, IMP⚠, INN⚠, LIZ, MIN, PIR, SNA, SPI, TIG⚠, UNI |
| charasis | 12 | CUB, DEV, DML, GOO, ICF, ICM, ICN, IKG, SIM, SSN, VRM, YET |
| chardok | 10 | FRG⚠, FRO⚠, ICN, SGO, SIM, SRW, SSK, SSN, VRM, WOF⚠ |
| chardokb | 1 | SSN |
| citymist | 9 | GOL, GOO, IKG, MEP, SGO, SIM, SRW, SSK, SSN |
| clb | 1 | CLB |
| cobaltscar | 13 | BAC, COF, COM, DRK⚠, FIS, MER, MIM, OTM, SED, SHA, SIR, WLM, WYV |
| codecay | 14 | BTX, BUB, BUU, DSB, KOP, LEP, MAL, NMG, NPT, PUS, SKB, SPD, VAC, WRF |
| cof | 1 | COF |
| com | 1 | COM |
| commons | 15 | BEA⚠, FPM, GHU, GIA, GRI, ORC, PIR, PUM, QCF, QCM, SNA, SPI, WIL, ZOF, ZOM |
| cpt | 1 | CPT |
| crb | 1 | CRB |
| crushbone | 12 | ALL⚠, CAZ⚠, EFR, FEM, FIS, GFM, INN⚠, KAM, KOB, ORC, PIR, RAT |
| crystal | 11 | COF, COM, DRF, DRM, FSG, GDM, ORC, RGM, SED, SPI, TEN |
| crystalshard | 11 | COF, COM, DRF, DRM, FSG, GDM, ORC, RGM, SED, SPI, TEN |
| cst | 1 | CST |
| cub | 1 | CUB |
| dalnir | 10 | BRN, DEV, DRF, DRM, GOO, ICF, ICM, KGO, SIM, SRW |
| dawnshroud | 8 | FIS, GAL, LCR, RHP, SGR, SHR, WET, ZEL |
| dji | 1 | DJI |
| dpf | 1 | DPF |
| dpm | 1 | DPM |
| dr2 | 1 | DR2 |
| dra | 1 | DRA |
| dreadlands | 11 | COC, DRA, DRF, DRM, FGI, KGO, LYC, RAP, SRW, SSK, YET |
| droga | 12 | BAC, BRN, CUB, ICF, ICM, ICN, KGO, LAUNCHM, QCF, SIM, SPI, SSN |
| drv | 1 | DRV |
| eastkarana | 14 | BEH, GIA, GNN, GRI, LIF, LIM, QCF, QCM, SNA, SPI, TRE, WIL, WOL⚠, ZOM |
| eastwastes | 19 | COF, COM, DIW, DR2, DRA, FSG, GDM, GRI, MAM, MTC, ORC, PUM, RHI, SBU, SDE, STG, TIG⚠, WAL, WLM |
| eastwastesshard | 19 | COF, COM, DIW, DR2, DRA, FSG, GDM, GRI, MAM, MTC, ORC, PUM, RHI, SBU, SDE, STG, TIG⚠, WAL, WLM |
| echo | 10 | FUG, MOS, MUH, REM, SGR, SHN, SHR, TEG, THO, UNB |
| ecommons | 18 | BEA⚠, BET, BIX, FPM, GHU, GRI, LIF, LIM, ORC, PUM, QCF, QCM, SNA, SPI, WIL, WOL⚠, ZOF, ZOM |
| eel | 1 | EEL⚠ |
| eey | 1 | EEY |
| efc | 1 | EFC |
| efe | 1 | EFE |
| efw | 1 | EFE |
| emeraldjungle | 14 | DRA, GOR, ICF, ICM, ICN, IKG, LEE, MEP, MOS, RAP, SIM, SSK, SSN, TIG⚠ |
| emp | 1 | EMP |
| epf | 1 | EPF |
| epm | 1 | EPM |
| erudnext | 22 | CAZ⚠, CPF, CPM, DJI, DRI, EFR, EGM, FIS, GAR, GOL, IMP⚠, INN⚠, KOB, LAUNCH, MER, PIR, PRE, REA, SEA, SHA, SHIP, SWO |
| erudnint | 15 | CAZ⚠, DEN, DER, DJI, DRI, EFR, EGM, FIS, GAR, GEF, GEM, GOL, IMP⚠, INN⚠, REA |
| erudsxing | 21 | BAT, BET, BGM, CPF, CPM, FIS, GEF, GEM, MER, PIF, PIR, PRE, QCM, SEA, SHA, SHIP, SNA, SPI, SWO, WIL, ZOM |
| everfrost | 11 | BEA⚠, GIA, GNN, GOB, HLF, HLM, MAM, ORC, PUM, SPI, WOL⚠ |
| fbf | 1 | FBF |
| fbm | 1 | FBM |
| fdr | 1 | FDR |
| fearplane | 20 | BEH, CAZ⚠, CUB, DEN, DRU, FRG⚠, GHU, GOL, GOR, GRG, IMP⚠, REA, SCA⚠, SPE⚠, SPI, TEN, UNI, WIL, ZOF, ZOM |
| feerrott | 21 | ALL⚠, BAT, BET, CAZ⚠, FIS, FRG⚠, FRT, FUN, GOR, INN⚠, LIZ, OKF, OKM, PIR, RAT, SNA, SPE⚠, SPI, TIG⚠, WOL⚠, ZOM |
| fel | 1 | FEL⚠ |
| felwithea | 15 | ALL⚠, CAZ⚠, DRI, FAF, FEF, FEM, FIS, GFF, GOL, IMP⚠, INN⚠, ORC, PIF, PIR, RAT |
| felwitheb | 16 | ALL⚠, CAZ⚠, DJI, DRI, EFR, FAF, FEF, FEM, FIS, GOL, IMP⚠, INN⚠, PEG, PIF, PIR, REA |
| fgh | 1 | FGH |
| fieldofbone | 13 | BET, BGG, BRN, DRA, GEF, GOR, ICF, ICM, ICN, SCR, SED, SPI, WOF⚠ |
| firiona | 14 | DRA, DRF, DRI, DRM, FEM, FGI, FRG⚠, FRO⚠, KGO, LEE, LYC, SCR, SHIP, WOL⚠ |
| fmo | 1 | FMO |
| fmp | 1 | FMP |
| fmt | 1 | FMT |
| freporte | 17 | BAT, BET, BGM, DER, FIS, FPM, GHU, ORC, PRE, QCF, QCM, RAT, SHA, SHIP, SNA, SWO, WOL⚠ |
| freportn | 11 | BGM, CAZ⚠, DER, FPM, INN⚠, ORC, PIR, QCF, QCM, RAT, SPI |
| freportw | 15 | AVI, BAT, BET, BGM, EFR, FPM, GOB, GOL, IMP⚠, ORC, QCF, QCM, RAT, SNA, WOL⚠ |
| frog_mount | 2 | FMT, HSF |
| frontiermtns | 8 | BRN, FGI, ICF, ICM, ICN, KGO, SRW, YET |
| frozenshadow | 18 | ABH, BAT, DRK⚠, EFR, ENA, GDM, GEF, GEM, GHU, GOM, GOO, HAG, LYC, SPC, VSF, VSM, ZOF, ZOM |
| fsk | 1 | FSK |
| fud | 1 | FUD |
| fungusgrove | 10 | FUG, GAL, KHA, MUH, NET, SHR, SKN, SNN, UNB, WET |
| gbl | 1 | GBL |
| gdr | 1 | GDR |
| gfaydark | 18 | BAT, BRF, BRM, DRI, FAF, FEM, GFF, GFM, GOL, INN⚠, ORC, PIF, SPI, TRE, UNI, WAS, WIL, WOL⚠ |
| ggl | 1 | GGL |
| glm | 1 | GLM |
| goo | 1 | GOO |
| gpf | 1 | GPF |
| gpm | 1 | GPM |
| greatdivide | 0 |  |
| gri | 1 | GRI |
| griegsend | 32 | AKM, FUG, GAL, GMF, GMM, GMN, KES, KHA, LCR, MUH, NET, OWB, REM, RHP, RNB, SCH, SDF, SDM, SGR, SHF, SHN, SHR, SKN, SNN, SOW, SPR, TEG, THO, UNB, VAC, VPM, ZEL |
| grimling | 8 | GMF, GMM, GMN, GSN, OWB, RHP, RNB, SOW |
| grobb | 11 | CAZ⚠, FIS, FRG⚠, FRO⚠, FRT, FUN, GRF, GRM, INN⚠, KOB, RAT |
| growthplane2 | 2 | BRF, BRM |
| growthplane | 13 | BEA⚠, DIW, ENA, MEP, PUM, SIR, TOT, TRE, UNI, WIL, WLM, WOL⚠, YAK |
| gtd | 1 | GTD |
| gukbottom | 16 | ALL⚠, BAT, BEH, CAZ⚠, FIS, FRG⚠, FRO⚠, FRT, FUN, GAR, GOL, INN⚠, MIM, MIN, REA, SPI |
| guktop | 15 | ALL⚠, CAZ⚠, FIS, FRG⚠, FRO⚠, FRT, FUN, GRM, INN⚠, KOB, LIZ, PIR, RAT, SPI, WIL |
| gunthak | 4 | HSF, MUH, REM, UDF |
| gvk | 1 | GVK |
| halas | 16 | BEA⚠, CAZ⚠, DJI, FIS, GHU, GNN, GOB, HLF, HLM, INN⚠, LAUNCH, PIR, PUM, WOL⚠, ZOF, ZOM |
| hateplane | 18 | DRU, DVM, GAR, GHU, GOL, IMP⚠, INN⚠, MIM, NGM, RAT, REA, SPE⚠, SPI, VSF, VSM, WIL, ZOF, ZOM |
| hateplaneb | 6 | DRU, GHU, GOL, TRK, VSF, VSM |
| hatesfury | 1 | SPB |
| highkeep | 12 | BGM, CAZ⚠, GHU, GNN, GOB, GOL, HHM, INN⚠, ORC, QCF, QCM, RAT |
| highpass | 13 | BAT, BEA⚠, BET, GNN, HHM, INN⚠, LIM, ORC, QCF, QCM, SPE⚠, SPI, WOL⚠ |
| hohonora | 8 | AAM, BKN, BWD, DR2, DRA, REM, RZM, WRU |
| hohonorb | 4 | AAM, BKN, MAR, WRU |
| hole | 10 | GEF, GEM, GHU, GOL, IMP⚠, MIM, SPI, VRM, ZOF, ZOM |
| hollowshade | 9 | GMF, GMM, GMN, OWB, RHP, RNB, SOW, STU, WET |
| hpf | 1 | HPF |
| hpm | 1 | HPM |
| hss | 1 | HSS |
| ibr | 1 | IBR |
| iceclad | 11 | DIW, FSG, GNN, GRI, LAUNCH, LYC, PUM, SDE, SHIP, STU, WAL |
| icy | 1 | ICY |
| iec | 1 | IEC |
| ifc | 1 | IFC |
| ihu | 1 | IHU |
| ila | 1 | ILA |
| ilb | 1 | ILB |
| innothule | 17 | ALL⚠, CAZ⚠, FRG⚠, FRO⚠, FRT, FUN, GRF, GRM, INN⚠, KOB, LIZ, OKF, OKM, RAT, SNA, ZOF, ZOM |
| isb | 1 | ISB |
| ise | 1 | ISE |
| iwf | 1 | IWF |
| iwh | 1 | IWH |
| iwm | 1 | IWM |
| izf | 1 | IZF |
| izm | 1 | IZM |
| jaggedpine | 18 | GNN, GRI, GSF, GSM, GSN, LIM, LUJ, MEP, NYD, NYM, PUM, QCF, QCM, SNA, STU, TRE, TRN, WIL |
| jkr | 1 | JKR |
| kael | 11 | COF, COM, DIW, DRA, EFR, ENA, FSG, GAM, GOM, STG, STM |
| kaelshard | 11 | COF, COM, DIW, DRA, EFR, ENA, FSG, GAM, GOM, STG, STM |
| kaesora | 8 | DEV, ICN, SIM, SPI, SSK, SSN, VST, XAL |
| kaladima | 9 | CAZ⚠, FIS, GOB, GOL, INN⚠, KA, KAF, KAM, ORC |
| kaladimb | 10 | CAZ⚠, FIS, GOB, GOL, INN⚠, KA, KAF, KAM, ORC, RAT |
| karnor | 11 | ICN, IKG, IKH, LYC, SGO, SIM, SRW, SSK, SSN, VST, XAL |
| katta | 13 | ALL⚠, BAT, GAR, KES, MIM, SDF, SDM, SOW, VOL, VPM, WET, ZOF, ZOM |
| kedge | 10 | BEH, DJI, FIS, GOB, KED, MER, PIR, SEA, SHA, SWO |
| kerraridge | 15 | CAZ⚠, CPF, CPM, DJI, EFR, FIS, GOR, INN⚠, KOB, LIF, LIM, PIR, PUM, RAT, TIG⚠ |
| kgo | 1 | KGO |
| kithicor | 18 | BAT, BEA⚠, BET, BIX, GHU, GOB, ORC, PIR, QCF, QCM, RAT, SNA, SPI, TIG⚠, WIL, WOL⚠, ZOF, ZOM |
| kob | 1 | KOB |
| krk | 1 | KRK |
| kurn | 12 | BRN, DER, FRG⚠, FRO⚠, FUN, GOO, MEP, SIM, SSK, SSN, STC, WIL |
| l01 | 1 | LAUNCH |
| lakeofillomen | 11 | BAC, BGG, ICF, ICM, ICN, KGO, SED, SGO, SRW, SSK, STC |
| lakerathe | 15 | ALL⚠, AVI, BET, BGM, FIS, FRG⚠, GNN, GOB, ORC, PIR, QCF, QCM, SHA, SNA, ZOM |
| lavastorm | 14 | ALL⚠, CAZ⚠, DER, DRK⚠, EFR, GAR, GOB, GOL, IMP⚠, INN⚠, QCF, QCM, SNA, UNI |
| letalis | 13 | HAR, REM, RGM, RHP, RNB, SDF, SDM, SGR, SKN, TEG, THO, UNB, VAC |
| lfaydark | 18 | BAT, BRF, BRM, CAZ⚠, CLM, DRI, FAF, INN⚠, ORC, PIF, SPI, TRE, UNI, VSF, VSM, WAS, WOL⚠, ZOM |
| lga | 1 | LGA |
| lgr | 1 | LGR |
| liz | 1 | LIZ |
| lmf | 1 | LMF |
| lmm | 1 | LMM |
| lug | 1 | LUG |
| luj | 1 | LUJ |
| maiden | 11 | AKM, AKN, BAT, GAL, KES, SDF, SDM, TEG, VAC, VOL, VPM |
| mal | 1 | MAL |
| mep | 1 | MEP |
| mim | 1 | MIM |
| mischiefplane | 14 | BIX, FUN, GOR, MEP, MIM, RAT, REA, RIM, SBU, SKU, SPH, UNI, VRF, VRM |
| mistmoore | 17 | CAZ⚠, DVM, FIS, GAR, GOL, IMP⚠, INN⚠, MIM, QCF, REA, SPI, UNI, VSF, VSM, WIL, ZOF, ZOM |
| misty | 17 | BAT, BEA⚠, BET, BIX, FRO⚠, GOB, ORC, RAT, RIF, RIM, SNA, SPI, UNI, WAS, WOL⚠, ZOF, ZOM |
| mmf | 1 | MMF |
| mmm | 1 | MMM |
| mmv | 1 | MMV |
| mmy | 1 | MMY |
| moi | 1 | MOI |
| mos | 1 | MOS |
| mseru | 12 | BGM, KES, LCR, REM, RHP, RNB, SGR, SKE⚠, SUC, TEG, UNB, ZEL |
| mtc | 1 | MTC |
| muh | 1 | MUH |
| najena | 15 | CAZ⚠, DJI, EFR, FRG⚠, GAR, GOB, GOL, IMP⚠, INN⚠, MIM, MIN, SPI, TEN, VSF, VSM |
| necropolis2 | 2 | BWD, GDR |
| necropolis | 16 | BAT, BET, CCD, COM, DR2, DRA, DRK⚠, DRU, GOO, RAT, SNA, SPI, TRK, VRM, WUR, WYV |
| nektulos | 17 | BEA⚠, BET, BIX, CAZ⚠, GOL, IMP⚠, INN⚠, NGM, ORC, PIR, SNA, SPI, UNI, WIL, WOL⚠, ZOF, ZOM |
| neriaka | 15 | BGM, CAZ⚠, EGM, FPM, GAR, GOL, IMP⚠, INN⚠, NGM, ORC, QCF, QCM, RAT, VSF, VSM |
| neriakb | 18 | ALL⚠, CAZ⚠, DEN, EFR, FIS, GAR, GOL, IMP⚠, INN⚠, NGM, ORC, PIR, RAT, REA, VSF, VSM, ZOF, ZOM |
| neriakc | 20 | ALL⚠, CAZ⚠, DEN, EFR, FIS, GAR, GHU, GOL, GRM, IMP⚠, INN⚠, MIM, NGM, ORC, PIR, RAT, REA, SPE⚠, VSF, VSM |
| netherbian | 10 | FUG, GAL, GMF, GMM, GMN, GOO, MUH, NET, TEG, VAC |
| nexus | 1 | MUH |
| nightmareb | 6 | GGL, NMH, NMW, TRT, UDF, UDK |
| nin | 1 | NIN |
| nmh | 1 | NMH |
| northkarana | 16 | BEA⚠, BET, BGM, CEN, GHU, GIA, GRI, LIF, LIM, PEG, QCF, QCM, TRE, WIL, WOL⚠, ZOM |
| nro2 | 1 | LAUNCH |
| nro | 16 | ARM, BET, BGM, DER, FPM, GHU, GIA, ORC, PUM, QCF, QCM, SNA, SPI, WOL⚠, ZOF, ZOM |
| nurga | 14 | ALL⚠, BAC, BRN, CUB, DRF, DRM, GOB, KGO, LAUNCHM, LEE, MOS, RAT, SPI, WOF⚠ |
| nyd | 1 | NYD |
| nym | 1 | NYM |
| oasis | 16 | ALL⚠, BET, BGM, DER, GHU, GIA, GOB, ORC, PUM, QCF, QCM, SNA, SPE⚠, SPI, ZOF, ZOM |
| oggok | 15 | ARM, BAT, CAZ⚠, FIS, FRG⚠, FRO⚠, FRT, GOR, GRM, IMP⚠, INN⚠, LIZ, OKF, OKM, RAT |
| oot | 15 | AVI, FIS, GAR, GIA, GOB, GSP, LIZ, MER, PRE, QCF, QCM, SHA, SHIP, SPE⚠, SWO |
| opf | 1 | OPF |
| opm | 1 | OPM |
| overthere | 15 | CLM, COC, GAR, GOL, ICF, ICM, ICN, ISC, NGM, PRE, RHI, SIM, SRW, STC, SUC |
| paf | 1 | PAF |
| paineel | 13 | BAT, BET, GEF, GEM, GHU, GOL, KOB, PIR, RAT, SNA, SPE⚠, ZOF, ZOM |
| paludal | 7 | FUG, OWB, SDF, SHR, SKN, UNB, WET |
| paw | 14 | BAT, BGM, CAZ⚠, CUB, FIS, GNN, IMP⚠, INN⚠, MIN, ORC, RAT, SNA, WOL⚠, ZOM |
| pbr | 1 | PBR |
| permafrost | 11 | BEA⚠, CAZ⚠, DJI, DRA, GIA, GOB, INN⚠, ORC, SPI, TEN, WOL⚠ |
| poair | 12 | AAM, AMP, DJI, EFC, EFE, ELE, PHX, QZT, SPD, SPL, STR, XEG |
| podisease | 18 | BTX, BUB, BUU, DSB, GOO, KOP, LCR, LEP, MAL, MIM, PUS, RAT, SLG, SPD, SPL, UNB, VAC, WRF |
| poeartha | 16 | ARM, CRO, EEL⚠, EMP, GOL, GSN, PAF, RGM, RTH, SGR, SMA, SPD, SPL, STA, TRN, VEG |
| poearthb | 6 | EMP, GOL, RTH, SMA, STA, VEG |
| pofire | 20 | BWD, COM, DR2, DRA, EFC, EFE, EFR, FEL⚠, FEN, FMP, GOL, GOM, GSF, GTD, NMH, PAF, PHX, SGR, SPD, SRG |
| poinnovation | 14 | BRC, CCD, CLB, CLF, CLG, CLM, CWB, GLC, GOO, JUB, RAT, SPD, TIN, TRK |
| pojustice | 40 | AKM, AVI, BEH, BET, CEN, DEN, DML, DRF, DRM, EFR, FRG⚠, FRO⚠, FUG, GAR, GDM, GEF, GEM, GIA, GNN, GOJ, GRG, KGO, MIN, ORC, OTM, RAT, RGM, SDF, SGR, SHN, SIM, SKB, SPH, SRW, SSN, TBU, THO, VAC, VRF, VRM |
| poknowledge | 23 | AKM, AKN, AVI, BRF, BRM, CEN, DRI, FAF, FDR, FRO⚠, GNN, KGO, KOB, MIN, NYM, ORC, OTM, PIF, RGM, SPH, SRW, STR, YAK |
| ponightmare | 21 | BRV, CRO, FRG⚠, GGL, HSN, LUJ, NBT, NMG, NMH, NMP, NMW, NPT, SKB, SLG, SPD, TRN, TRT, UDF, UDK, VEG, WET |
| postorms | 31 | ARM, DER, FGI, FIS, FMP, FRO⚠, FRT, GIA, KAR, LIM, MEP, MUH, PAF, PBR, PEG, REM, SCE, SCR, SKR, SMA, SNA, SSA, STA, STC, STF, STR, SUC, SVO, SWO, TOT, TRN |
| potactics | 20 | BRV, DML, GOO, GTD, KOB, MUH, RAL⚠, RAZ, RZM, SDF, SDM, STG, TAZ, TRW, VAL, VAZ, VEG, WBU, WRB, WRW |
| potimea | 13 | AAM, BKN, GOL, GRI, LIM, MAM, PUM, RGM, SGR, SHN, SMA, SPD, SPH |
| potimeb | 30 | ABH, BTX, CRB, CUB, DEN, DSB, FMP, KOP, LEP, MOI, NMG, PAF, PHX, RAZ, RZM, SAR, SGR, SLG, TAZ, TMT, TRT, TRW, UDF, UDK, VAZ, VSF, VSM, WRB, WRW, ZBC |
| potorment | 7 | BRV, GSN, IVM, MOI, SAR, SPD, TRW |
| povalor | 14 | AAM, BKN, FUG, IKG, LCR, PAF, PRI, SKE⚠, SPD, SPL, TRK, UDF, UDK, VAC |
| powater | 15 | BAC, CRB, FIS, FRT, GSM, KRK, PAF, PIR, SED, SHA, SLG, STU, SWO, TMR, WMP |
| pri | 1 | PRI |
| pum | 1 | PUM |
| pus | 1 | PUS |
| qcat | 18 | ALL⚠, BAT, BET, BGM, CAZ⚠, CUB, FIS, FRG⚠, INN⚠, MIN, PIR, QCF, QCM, RAT, SHA, SNA, SPI, ZOM |
| qey2hh1 | 15 | BEA⚠, BET, GHU, GIA, GNN, LIF, LIM, QCF, QCM, SCA⚠, SPI, TRE, WIL, WOL⚠, ZOM |
| qeynos2 | 14 | BAT, BEA⚠, BET, BGM, CAZ⚠, FIS, GNN, IMP⚠, INN⚠, QCF, QCM, RAT, SNA, ZOM |
| qeynos | 17 | BEA⚠, BGM, CEN, FIS, GNN, GOL, GOR, LIM, MIN, PIR, PRE, QCF, QCM, RAT, SHA, SHIP, TIG⚠ |
| qeytoqrg | 15 | BAT, BEA⚠, BET, CAZ⚠, FIS, GNN, INN⚠, PIR, QCF, QCM, RAT, SNA, WIL, WOL⚠, ZOM |
| qrg | 16 | BAT, BEA⚠, BRF, BRM, CEN, FAF, FIS, GNN, PIF, PIR, QCF, QCM, TRE, UNI, WIL, WOL⚠ |
| rak | 1 | RAK |
| rap | 1 | RAP |
| rathemtn | 14 | ALL⚠, BEA⚠, BET, DRK⚠, GIA, LIZ, ORC, PUM, QCF, QCM, SPE⚠, SPH, ZOF, ZOM |
| rea | 1 | REA |
| rem | 1 | REM |
| rgm | 1 | RGM |
| rif | 1 | RIF |
| rivervale | 18 | BEA⚠, BIX, BRF, BRM, CAZ⚠, DRI, FAF, FIS, GOB, INN⚠, ORC, PIF, PIR, RAT, RIF, RIM, SCA⚠, WOL⚠ |
| roe | 1 | ROE |
| rom | 1 | ROM |
| runnyeye | 15 | BEH, CAZ⚠, CUB, FIS, FUN, GAR, GOB, GOL, IMP⚠, INN⚠, MIM, MIN, ORC, PIR, SNA |
| scarlet | 7 | GAL, KHA, LCR, RHP, SCH, SNN, TEG |
| scr | 1 | SCR |
| sdc | 1 | SDC |
| sdf | 1 | SDF |
| sdm | 1 | SDM |
| sebilis | 11 | BET, CUB, FRG⚠, FRO⚠, FUN, ICF, ICM, ICN, IKG, SIM, TRK |
| sed | 1 | SED |
| sef | 1 | SEF |
| sem | 1 | SEM |
| sgr | 1 | SGR |
| shadeweaver | 10 | KES, MIM, RHP, RNB, SKN, SNN, TEG, UNB, VAC, VPM |
| shadowhaven | 1 | MUH |
| sharvahl | 10 | GMF, GMM, GMN, KES, MIM, RHP, RNB, SCR, SOW, VAC |
| shm | 1 | SHM |
| shp | 1 | SHP |
| shr | 1 | SHR |
| sif | 1 | SIF |
| sim | 1 | SIM |
| sir | 1 | SIR |
| sirens | 10 | BAC, FIS, MER, SEA, SHA, SIR, SWO, WAL, WIL, WLM |
| skb | 1 | SKB |
| skn | 1 | SKN |
| skt | 1 | SKT |
| skyfire | 6 | DEV, DRA, DRK⚠, FDR, WUR, WYV |
| skyshrine2 | 2 | BWD, PRI |
| skyshrine | 14 | COM, CUB, DR2, DRK⚠, DRU, FSG, GAM, GOM, KOB, SED, SPI, STG, WUR, WYV |
| sleeper2 | 1 | PRI |
| sleeper | 9 | DR2, DRA, DRK⚠, DRU, GAM, GOM, SED, WUR, WYV |
| snn | 1 | SNN |
| soldunga | 13 | BAT, CAZ⚠, CLF, CLM, DRK⚠, EFR, GOB, GOL, IMP⚠, INN⚠, KOB, RAT, SPI |
| soldungb | 13 | BAT, BET, CAZ⚠, DRA, DRK⚠, EFR, GAR, GIA, GOB, IMP⚠, INN⚠, KOB, SPI |
| solrotower | 21 | BWD, DER, DJI, DR2, DRA, DRK⚠, EFE, EFR, FEL⚠, FMP, GOL, GSF, GTD, NMH, PAF, PHX, RGM, SRG, SRO, STF, WYV |
| soltemple | 2 | EFR, IMP⚠ |
| southkarana | 14 | AVI, CEN, GIA, GNN, HLF, HLM, INN⚠, LIF, LIM, MAM, PEG, TRE, WOL⚠, ZOM |
| spd | 1 | SPD |
| sro | 17 | ALL⚠, BET, BGM, CAZ⚠, DER, DJI, EFR, GHU, GIA, INN⚠, ORC, PUM, SNA, SPI, WOL⚠, ZOF, ZOM |
| sseru | 8 | LCR, MIM, NET, REM, RNB, SER, SKN, ZEL |
| ssratemple | 6 | IKG, KHA, SHF, SHM, SHN, THO |
| steamfont | 18 | AVI, BGM, BRF, BRM, CAZ⚠, CLF, CLM, DRK⚠, EFR, HAR, IMP⚠, INN⚠, KOB, MIN, PUM, RAT, SPI, UNI |
| stf | 1 | STF |
| stonebrunt | 15 | CPF, CPM, GEF, GEM, GOR, KOB, LIF, LIM, PUM, SBU, SNA, STC, TIG⚠, WIL, WLM |
| str | 1 | STR |
| stu | 1 | STU |
| swampofnohope | 11 | DEV, FRG⚠, FRO⚠, ICF, ICM, ICN, KGO, LEE, MEP, MOS, SRW |
| syn | 1 | SYN |
| tac | 1 | TAC |
| tbf | 1 | TBF |
| tbl | 1 | TBL |
| tbm | 1 | TBM |
| tef | 1 | TEF |
| tem | 1 | TEM |
| templeveeshan | 15 | COF, COM, DR2, DRA, DRK⚠, ENA, FDR, RAP, SBU, SED, STG, STM, TRK, WUR, WYV |
| ten | 1 | TEN |
| tenebrous | 11 | BAT, GMF, GMM, GMN, KES, SDF, SDM, SOW, TEG, VOL, VPM |
| tgl | 1 | TGL |
| thedeep | 10 | FUG, MUH, REM, SHF, SHM, SHN, SHR, THO, UNB, VAC |
| thegrey | 7 | GSN, IKG, IKS⚠, SGR, SHN, SIM, VAC |
| thurgadina | 10 | BAT, COF, COM, DIW, FSG, GDM, GOO, OTM, RAT, SPI |
| thurgadinb2 | 1 | COK |
| thurgadinb_2 | 1 | COK |
| thurgadinb | 7 | COF, COM, CUB, GDM, GOM, TEN, WLM |
| timorous2 | 1 | LAUNCHM |
| timorous | 16 | ALL⚠, AVI, BAC, BGG, DRK⚠, ICF, ICM, ICN, IKG, LAUNCH, LAUNCHM, PRE, RAP, SED, SHIP, YET |
| tmb | 1 | TMB |
| tmt | 1 | TMT |
| tnf | 1 | TNF |
| tnm | 1 | TNM |
| torgiran | 4 | KOB, MUH, REM, WEL⚠ |
| tox | 21 | BET, CAZ⚠, CPF, CPM, DRI, EGM, FAF, FIS, GOL, GOR, INN⚠, KOB, PIF, PIR, RAT, SKU, SNA, SPI, TIG⚠, WAS, WIL |
| tpb | 1 | TPB |
| tpo | 1 | TPO |
| trakanon | 11 | COC, FRG⚠, FRO⚠, ICF, ICM, ICN, MEP, RAP, SIM, SRW, STC |
| trk | 1 | TRK |
| trn | 1 | TRN |
| trw | 1 | TRW |
| tsm | 1 | TSM |
| ttb | 1 | TTB |
| tvp | 1 | TVP |
| twf | 1 | TWF |
| twilight | 10 | AKM, BAC, BGG, PIR, RNB, SDF, STU, TEG, UNB, WET |
| tzf | 1 | TZF |
| tzm | 1 | TZM |
| udk | 2 | FBF, UDK |
| umbral | 11 | AKM, GAL, GSF, GSM, GSN, KES, NET, SDF, TEG, THO, ZEL |
| unb | 1 | UNB |
| unrest | 21 | BET, BGM, CAZ⚠, DER, FUN, GDM, GHU, GOL, IMP⚠, INN⚠, KOB, MIM, MIN, PIR, REA, SCA⚠, SPE⚠, TEN, WIL, ZOF, ZOM |
| vac | 1 | VAC |
| vas | 1 | VAS |
| veeshan | 26 | BON, BOX, BRL, BWD, CCD, CST, DR2, DRA, DRK⚠, DRU, FDR, GDR, GEF, GEM, PRI, RAK, RAP, SED, SIM, SSN, TBL, TMT, TRK, VAS, WUR, WYV |
| veg | 1 | VEG |
| vek | 1 | VEK |
| velketor | 0 |  |
| vexthal | 10 | AKM, AKN, GOO, GSF, GSM, GSN, MIM, SDF, SDM, SGR |
| wakening | 16 | BRF, DR2, DRA, DRI, ENA, FDR, FMO, FSG, GOO, PIF, PUM, RAP, RGM, STG, UNI, YAK |
| warrens | 9 | BAT, CPF, CPM, FRT, KOB, PIF, RAT, WLM, WOL⚠ |
| warslikswood | 10 | FGI, ICF, ICM, ICN, KGO, SED, SIM, SSN, WOF⚠, YET |
| wbu | 1 | WBU |
| wel | 1 | WEL⚠ |
| westwastes | 14 | BTM, CCD, COF, COM, DIW, DR2, DRA, DRK⚠, FSG, LEE, MAM, STG, WUR, WYV |
| wmp | 1 | WMP |
| wof | 1 | WOF⚠ |
| wrf | 1 | WRF |
| wuf | 1 | WUF |
| wyv | 1 | WYV |

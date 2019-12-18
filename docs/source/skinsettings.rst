Skin Settings
=============
Skin specific settings that can be defined in ``config-definitions.json`` in the
root folder of your skin.

The structure of the json looks like this:

.. code-block:: json

	{
		"key" : {
			"type" : "selection/text/label/bool/int/float/color",
			"default" : "default_value",
			"label" : "not required, key will be used as label if missing"
		},

		"separator_XXX" : {},

		"Label Text" : { "type" : "label" }
	}


Getting and setting values from lua
***********************************

game.GetSkinSetting(string key)
-------------------------------
Returns the value of the setting with the given key, returns nil if the key
can't be found.

game.SetSkinSetting(string key, number/boolean/string value)
------------------------------------------------------------
Sets the value of the settings entry with the given key, you must use the
same type as the key to set the value. So you cannot set a number entry
using the value ``"some text"`` for example.

selection
*********
``selection`` entry type. Required fields:

- ``default`` : ``string value``
- ``optioons`` : ``array of strings``

text
****
``text`` entry type. Required fields:

- ``default`` : ``string value``

Optional fields:

- ``secret`` : ``true/false``

color
*****
``color`` entry type. Required fields:

- ``default`` : ``hex string formatted "RRGGBBAA"``

bool
****
``bool`` entry type. Required fields:

- ``default`` : ``true/false``


float
*****
``float`` entry type. Required fields:

- ``default`` : ``default value``
- ``min`` : ``minimum allowed value``
- ``max`` : ``maximum allowed value``

int
***
``int`` entry type. Required fields:

- ``default`` : ``default value``
- ``min`` : ``minimum allowed value``
- ``max`` : ``maximum allowed value``

label
*****
``label`` entry type has no required fields except the type field. The key
will be used as a label on the settings screen.

separator
*********
To place a separator on the settings screen you just have to create an
empty entry that has a key that starts with ``separator`` but all your separators
need to have unique keys for them to display.

Example
*******
The default skin comes with an example that has every available config entry type.
It looks like this

.. code-block:: json

	{
		"Gameplay:" : { "type" : "label" },

		"earlate_position": {
			"type": "selection",
			"label": "Early/Late display position",
			"default": "bottom",
			"values": ["bottom", "middle", "top", "off"]
		},
		"nick": {
			"type" : "text",
			"label" : "Display name",
			"default" : "Guest"
		},

		"separator_a" : {},
		"Song select:" : { "type" : "label" },
		"show_guide": {
			"label" : "Show control guide on song select",
			"type": "bool",
			"default": true
		},
		"separator_b" : {},
		"Test objects:" : { "type" : "label" },
		"Testing with space" : {
			"type": "float",
			"label": "Test setting with spaces in the key",
			"default": 50.0,
			"max": 100.0,
			"min": -100.0
		},

		"Ineger_test" : {
			"type": "int",
			"label": "Ineger Test with range -100<->100",
			"default": 50,
			"max": 100,
			"min": -100
		},

		"col_test" : {
			"type": "color",
			"label": "Color Test",
			"default": "007FFFFF"
		},

		"secret_value" : {
			"type" : "text",
			"label" : "Secret value test",
			"default" : "usc123",
			"secret" : true
		}
	}

Challenge File Format
=====================

.. |br| raw:: html

  <br/>
  
You can use this tool to easily create challenges and courses: https://itszn.github.io/usc_db_tool/

USC challenges are stored in files with extension ".chal". These are JSON files with the following structure:

.. code-block:: javascript

  {
      "title": "Some title",
      "level": 11, // Level is from 1-12 (inf)
      "charts": [ /* see Chart section below */ ],
      "global": {
            /* See Settings section below */
      },
      "overrides": [ /* see Overrides section below */ ]
  }

The file must contain :code:`title`, :code:`level`, :code:`charts`, and :code:`global`. :code:`overrides` is optional

**IMPORTANT NOTE:** comments are used for these docs, but the actual JSON file does not allow comments! Additionally strict JSON spec must be followed so no extra commas at the end of a list are allowed!

Charts
******

The :code:`charts` entry is a list of charts to load. Each entry can take three forms:

1. Path of chart ending in ".ksh"

   This will be used to look up a chart with a specific path. This is also used when converting ".kco" files which use path to specify charts (make sure to use \\ if on windows)

   .. code-block:: javascript
   
     "charts":[
         "SDVX III GRAVITY WARS\\Destroy\\grv.ksh"
     ]

2. Hash of chart

   This will look up charts with a given hash. Note that if a chart has been changed at all the hash will also change. This is most useful for distributing charts and challenges at the same time since it doesn't rely on the user's directory structure

   .. code-block:: javascript
   
     "charts":[
         "3c5fe60481790f658412909b39f464f2412622a5"
     ]

3. Name and Level

   You can select a chart based on just its title (or a substring of its title) and level by using an object with both :code:`name` and :code:`level` filled in.

   .. code-block:: javascript
   
     "charts":[
         {"name":"black lotus","level":16}, // Case insensitive
         {"name":"び","level":18} // Can include utf-8 here
     ]

If no chart is found the challenge will have the :code:`missing_chart` field set and be unplayable


Settings
********

There are several categories of settings: Requirements, Overall Requirements, Options, and Non-Overrideable Options

Overrideable Options 
------------------------

These options can be set globally or per chart

**excessive_gauge** (bool)

 | Force use of excessive gauge

**permissive_gauge** (bool)

 | Force use of permissive gauge

**blastive_gauge** (bool)

 | Force use of blastive gauge

**gauge_level** (float)

 | Set the blastive level

**ars** (bool)

 | Force use of ARS (backup gauge)

**mirror** (bool)

 | Force use of mirror mode

**near_judgement** (int) |br|
**crit_judgement** (int) |br|
**hold_judgement** (int)

 | Set the judgement windows

**min_modspeed** (int 100-1000) |br|
**max_modspeed** (int 100-1000)

 | Set the min and max that modspeed can be during a chart (either can also be omitted). The player will not be able to make the speed faster than the max or slower than the min. If the song bpm changes the speed to be outside this range, the hispeed will be adjusted accordingly to force it back inside.

**allow_cmod** (bool)

 | If set cmod will not be allowed for the challenge and mmod will be used instead

**allow_effective** (bool default true)

 | If true, challenge can be started with effective gauge

**allow_permissive** (bool default false)

 | If true, challenge can be started with permissive gauge

**allow_blastive** (bool default false)

 | If true, challenge can be started with blastive gauge

**allow_ars** (bool default true)

 | If true, challenge can be started with backup gauge enabled

**hidden_min** (float) |br|
**sudden_min** (float)

 | Force hidden and/or sudden to fall into a specific range. This will enable hidden/sudden if set



Non-Overrideable Options
------------------------

These options can't be overridden per chart

**use_sdvx_complete_percentage** (bool default false)

 | If true, failed charts will use partial completion based on how far through the cart the player was. If false, the percent will always be based only on score

**gauge_carry_over** (bool default false)

 | If true, gauge will not reset on the next chart. (ie if you have a 32% after chart 1, chart 2 starts with 32%)


Overrideable Requirements
-------------------------

These are requirements each chart must meet to pass. The can be overridden on a per chart basis

**clear** (bool)

 | If true, charts must be cleared (>=70% normal gauge or >0% excessive gauge) to pass. Can be turned off to allow failing charts to pass challenge requirements

**min_percentage** (int 0-200)

 | Minimum overall completion percentage needed to pass the chart. Percentage ranges from 0 to 200 for scores of 8mil -> 10mil. (8.5mil = 50%, 9.5mil = 150% etc). On failed charts the percentage depends on the :code:`use_sdvx_complete_percentage` option above.

**min_gauge** (float 0-1.0)

 | The minimum final gauge score required to pass the challenge. Can be used to make effective clears harder (note this does not change when mid-chart excessive fails happen, which will still happen at 0%)

**max_errors** (int) |br|
**max_nears** (int)

 | Sets a per chart max on errors or nears. If more are gotten on a single chart the chart will not pass the challenge

**min_crits** (int)

 | Sets a per chart min on crits. If less are gotten on a single chart the chart will not pass the challenge

**min_chain** (int)
 | Set a per chart minimum chain needed to pass. If not gotten at some point in the chart, the chart will not be passed.


Overall Requirements
-----------------------------

These requirements are based on the total performance on all charts and are only evaluated if the player passes all set per-chart requirements above. Note: these cannot be overriden per chart since they are based on all charts played

**min_average_gauge** (float 0-1.0)

 | The minimum average final gauge required to pass

**min_average_percentage** (int)

 | Average clear percentage required. See :code:`min_percentage` above for more details

**max_average_errors** (int) |br|
**max_average_nears** (int) |br|
**min_average_crits** (int)

 | Max/min number of errors|nears|crits gotten on average. For more fine control use :code:`*_overall_*` below

**max_overall_errors** (int) |br|
**max_overall_nears** (int) |br|
**min_overall_crits** (int)

 | Max/min number total of errors|nears|crits. The total is just the sum of the stat from each chart



Overrides
*********

The :code:`charts` entry is a list of setting overrides per chart. Each entry is an object with settings that should be overriden. To skip a chart use an empty object (ie :code:`{}`).

:code:`null` can also be set to disable the requirement or option for the given chart.

Example:

.. code-block:: javascript

  "overrides": [
	// Overrides for the first chart
    {
        "min_gauge": 0.5, // Change the min gauge requirement for the first chart
        "max_modspeed": 100 // Change max_modspeed option for the first chart
    },
    // Skip second chart
    {},
    // Overrides for third chart
    {
        "max_errors": null, // Do not require a max errors for this chart
        "clear": false // Do not require this chart to be cleared
    }
    //etc
 }


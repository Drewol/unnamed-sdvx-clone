Settings Wheel
==============
The wheel for setting settings such as mirror,random and hard gauge.

Contains ``settings`` which has the following fields:

.. code-block:: c#

    int currentSelection

    //Array with the name and value of all settings accessed directly under the settings table
    Setting[]

Setting
*******
Contains the following fields:

.. code-block:: cpp

    string name
    bool value

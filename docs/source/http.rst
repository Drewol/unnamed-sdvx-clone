Http
====
This section contains the documentation for the functions that can be found under `Http`

You should generally not do anything critical with http requests as you should not expect users to always be connected.
The non async functions block until the request has been completed so those should not be used in the main loop of your
code. The async functions queue the request on a separate thread, every async call is executed in the order they were made in
and the callbacks are done on the main thread so for performance reasons you should try not to do anything too heavy on callbacks
as well.


Example::

    function gotResponse(response)
        game.Log(string.format("Got response from '%s' with status code '%d'", response.url, response.status),
            game.LOGGER_INFO)
        game.Log(response.text, game.LOGGER_INFO)
    end

    header = {}
    header["user-agent"] = "usc v0.3.1"
    Http.GetAsync("https://httpbin.org/get", header, gotResponse)

response
********
A response object contains the following

.. code-block:: c#

    string url
    string text
    int status
    double elapsed
    string error
    string cookies
    map<string, string> header


Get(string url, map<string,string> header)
******************************************
Executes a blocking HTTP GET request and returns a ``response``.

Post(string url, string content, map<string,string> header)
***********************************************************
Executes a blocking HTTP POST request and returns a ``response``.

GetAsync(string url, map<string,string> header, function callback)
******************************************************************
Executes a HTTP GET request and calls the ``callback`` with the ``response`` as a parameter.

PostAsync(string url, string content, map<string,string> header, function callback)
***********************************************************************************
Executes a HTTP POST request and calls the ``callback`` with the ``response`` as a parameter.

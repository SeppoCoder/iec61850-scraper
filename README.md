# IEC61850 scraper

**Disclaimer!**

The main purpose of this repository is to act as a download page.<br> The repository does **not** currently include build scripts and/or Makefiles for developers wanting to continue the development of this software. 

## About

The **IEC61850 scraper** is a tool designed to automate the hassle of taking inventory of your Intelligent Electronic Devices (IEDs), which talk over MMS via the IEC61850 standard.

It works by scanning your local area network for IEDs, connecting to them via MMS, and then retrieving the information wanted, which is specified by the user in the config file. 

This information is then automatically uploaded and formated to an excel spreadsheet. The scraper logs all events in a log file, which enables you to know exactly what has happend during the scrapers runtime. 

## How to Use

To begin you must download the latest version of the .exe file. Before you run the program, you need to make sure that you have a config.json file in the same directory as the executable. You can either create one of your own config files, or download the one found in this repository. Make sure to update the different attributes in the config file to match the structure of your devices. 

Once ran, the scraper may ask which network you would like to scan if it can't find any of the predefined ones.

When the scraper is finished, it will output both the excel spreadsheet and the log file to the same directory as you ran the scraper in. 

### Config File

To fully automate the process, you can add and remove paths to data attributes in the config.json file. It works by giving a spreadsheet title and a list of paths to the data attribute you would like to retrieve. The scraper will try to fetch data from all the paths until some valid data is found for that attribute. 

The config file also enables you to give paths to data attributes for certain devices. If you e.g. have only a pair of devices which has a certain attribute, then you can add that attribute to only those IP-addresses. 

A **general path** looks like the following:

```
"Example Title" : ["LN0.DO0.DA0", "LN1.DO1.DA1", ... ],

                        .
                        .
                        .
```

Where as a **user specific path** looks like this:

```
{
    "Title" : "Example Title",
    "Path" : "LN_XX.DO_XX.DA_XX",
    "IpAddress" : ["X.X.X.X", "Y.Y.Y.Y", ...] 
}, 
                        .
                        .
                        .
```

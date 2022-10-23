
# Printer-Setup-Tool

Code and documentation for the work done in Google Summer of Code 2022 under The Linux Foundation for a GTK-based tool to setup IPP devices using IPP System Service.

## Introduction

A lot of users expect that if they plug in a USB printer that it is immediately available for use, without need to set it up. It is, therefore, important to build a printer setup system with a GTK-based GUI that can be embedded in the GNOME Control Center to bring all printer management control in one place. Currently, local print queues are managed using local printer tools, while administration interfaces run on browsers. To eliminate these issues, IPP System Service Standard which uses IPP requests to poll printer capabilities and configure the printer, thus providing a universal solution for IPP-compliant printers. Thus, the idea is to integrate various important components together to automate the process of setting up a printer without facing any obstacles. 

## Process Involved

The program checks for running IPP System Services by using avahi based service browsers and resolvers. The information received is then used to create IPP System Object uri(s). Then using IPP Requests, attributes of these System Objects and their component devices are obtained and stored in memory.

All of these objects are then displayed in the GUI in a hierarchical fashion using TreeViews. Sorting functionality is also implemented. Selecting any object displays its attributes in the sidebar. The program updates the object list in real-time by listening to service browser events.

The program has been fine-tuned and tested with multiple System Object and Printer Object instances.

## Workflow

- **system-services-show.c** sets up the GUI in its main function and creates an avahi service browser to browse services of type "_ipps-system._tcp"
- The service browser listens for events and creates separate service resolvers for all AVAHI_BROWSER_NEW and AVAHI_BROWSER_REMOVE events.
- In case of an AVAHI_BROWSER_NEW event, new IPP System Objects are created and for every new system object, 
    - A Get-System-Attributes request is issued using *get_attributes* method in **cupsapi.c** and attributes from the response are recorded.
    - A Get-Printers request is issued using *get_printers* method in **cupsapi.c** which is used to get component printer-uris, and then for every component printer, a Get-Printer-Attributes request is issued using *get_attributes* method and attributes from the responses are recorded to create Printer Objects. These Printer Objects are stored in a list inside their parent System Object.

    All of these new IPP Objects are added to the tree store of the GUI to display them in the application.

- In case of an AVAHI_BROWSER_REMOVE event, after confirming that a System Object no longer exists, it and all of its children Objects are freed and removed from the GUI.

## Files

`system-services-show.c` - Sets up the GUI for the project and detects Browsing and Resolving browser events to find system service instances.

`cupsapi.c` - Contains functions to make IPP Requests and parse IPP responses and gateway for communication between GUI and IPP Objects.

`printer_setup_gui.h` - Header file which includes all the libraries required to compile the code and defines structs and enums used throughout this project.

`system-services-show.sh` - Compiles and runs the program.


## Future Work

Functionality of discovering non-driverless printers and finding suitable printer applications for them needs to be integrated to the program at this point.

## Installation
gtk-4+ dev toolkit is required to build this program. 
Also need avahi-client, avahi-glib, and avahi-core libraries for DNS-SD and glib/gtk integration

#### Ubuntu
```
sudo apt-get install libgtk-4-dev
sudo apt-get install libavahi-client-dev
sudo apt-get install libavahi-glib-dev
sudo apt-get install libavahi-core-dev
```

## Testing and Running

There must be discoverable IPP System Services running on the network. [Pappl](https://github.com/michaelrsweet/pappl) can be used for this. 

Clone, configure and make pappl. Then run a Test System Service by executing testpappl file inside testsuite directory (See testpappl --help to see what flags can be used.):

```
pappl/testsuite/testpappl
```

Now we can run this program. Execute (inside this project's directory):
```
./system-services-show.sh
```

## Acknowledgements

I would like to thank all my mentors for helping me out with this project. A special thanks to my mentor Mr. Till Kamppeter, who has been there to answer all my queries and help me out everytime I got stuck. I am thankful to have gotten this project and it was a great learning experience.

# Adaptive Tools
## Overview
This repo contains the source for the adaptivemm and adaptived tools. 

[adaptivemm](./adaptivemm/README.md) monitors a machine's memory usage to track the rate of page consumption. It then uses this information to predict future memory usage and adjusts the watermark scale factor sysctl to kick off proactive background memory reclamation. This is done to avoid costly synchronous memory reclaim which can stall applications.

[adaptived](./adaptived/README.md) is a cause-and-effect daemon in which a user can specify their own causes which are used to decide if a certain action should be done in reponse to this cause.
 <br/>

 Further information for both tools is found in their respective directories.

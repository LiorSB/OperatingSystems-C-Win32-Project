# OperatingSystems-C-Win32-Project
Multi-threaded and multi-process program built with C and Win32 API. In the project there was also use of Mutexes, Semaphores, process inheritance and creation.
The project demanded to build a resemblance to passing vessels in the Suez Canal. In the project each vessel is a thread, and they start sailing at Haifa port (process), their 1st goal is to pass through the canal (with anonymous pipes) to Eilat port (process). In Eilat the vessels need to enter a synchronization point which after a set number of vessels have arrived, they can continue to the unloading quay in Eilat port (an ADT which is built with a thread). In the unloading quay each vessel stations itself near a crane (each crane is a thread) and from there they start their unloading process. Once a crane is done unloading the vessel’s cargo the vessel may return to Haifa port through the canal (anonymous pipe) and end all its work there.

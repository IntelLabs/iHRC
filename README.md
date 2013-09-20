Overview
========

Heterogeneity is ubiquitous. In client domains such as laptops, ultrabooks, and tablets, standard processors from major hardware vendors are now shipped with GPUs (For example, Intel's 3rd and 4th generation Core processors integrate a GPU on the same die as the CPU). Similarly, in server domains such as exascale systems, multicore CPUs within a node are embedded with GPU cards. Although, the emergence of portable languages like OpenCL have made these heterogeneous devices more accessible, programmers still need to learn low-level OpenCL APIs and more importantly, write different code targetting different devices of a heterogeneous system. Thus, existing multicore C/C++ applications need to be redesigned and written from scratch using OpenCL to target GPUs. Rewriting from scratch has significant impact on software engineering especially for large complex irregular algorithms.
 
The goal of Intel Lab's iHRC project is to enable existing parallel C/C++ workloads to execute on Intel integrated GPUs with minimal programming effort. To that extend, we introduce a high-level data parallel construct, named 'parallel_for_hetero', in C++ that (1) by using a compiler, generates low-level OpenCL code automatically for the GPU, and (2) by using a runtime, decides to execute on the GPU if the GPU is not busy.

Getting Started with the prototype
==================================

Please see INSTALL.txt at the top level.


Discussion
==========
Please direct your questions and answers to ihrc-discussion@googlegroups.com or visit the website https://groups.google.com/d/forum/ihrc-discussion.





===========
Neurocursor
===========

First milestone
============

Our project won first place among school projects.

.. image:: first.png
  :scale: 50%

Introduction
========

Neurocursor is a device based on the ATmega32U4 that provides an electromyographic human-machine interface for people with disabilities. It was developed as part of the `Neurotech <https://neurotechcup.com/en>`_ contest.

Algorithm
========

Capture
----

The data processing with the EMG board is fetched from the port and put into the stack.

Filtering
----------

Information from the stack is filtered, averaged. Also the `motor unit <https://wikiless.org/wiki/Motor_unit?lang=en>`_ is calculated from the frequency of the signal.

Processing
---------

The muscle tension is found by passing the threshold, and the direction is chosen according to the muscle tension and its motor unit.                                                                                                                                           
                                                                                                                                             
Action                                                                                                                                     
--------                                                                                                                                     
                                                                                                                                             
After processing, the mouse is moved in the specified direction.                                                                        
                                                                                                                                             
Future                                                                                                                                      
=======                                                                                                                                      
                                                                                                                                             
As with the `Stockfish <https://stockfishchess.org/>`_ project, the algorithm is going to be replaced by a neural network, more flexible and accurate. Also at the moment is not optimal implementation of the stack. The development of the project will continue in the main branch.                                                                                                          
                                                                                                                                             
Telegram contact information                                                                                                                           
=============================                                                                                                         
                                                                                                                                             
Egor Guslyantsev - @egorguslyan  

Ivan Zubkov - @kyborg_zlodey   

Peter Orekhov - @the_Yoker    

Ilya Treier - @SixSolid

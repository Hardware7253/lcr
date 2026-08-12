# LCR Meter
<img width="4032" height="3024" alt="lcr_1" src="https://github.com/user-attachments/assets/c583807d-f87d-4759-bb7a-18ddf8a3d8a3" />

## Description
This LCR meter is capable of measuring complex impedance of various passive components and features auto-ranging.
Figure 1 shows the basic test circuit and provides an equation that can be used to calculate the impedance of the DUT.
This circuit + additional filtering and biasing is used within the LCR design.
The MCU software uses the pk-pk values of the sampled waveforms for magnitudes, and calculates the phase difference using in-phase and quadrature components of the multiplied current and test voltage waveforms (v3 & v2). The IQ components are low-pass-filtered, then their arctan is taken to find the phase difference.

<img width="1498" height="707" alt="lcr_circuit" src="https://github.com/user-attachments/assets/930724d5-d4d1-4a2e-ba12-9c367944589a" />
Figure 1: Basic LCR testing circuit.

## Demos
<img width="4032" height="3024" alt="lead_test" src="https://github.com/user-attachments/assets/7d257890-4ab3-4ef2-8af1-c887076a70b1" />
<img width="4032" height="3024" alt="r_test_4r7" src="https://github.com/user-attachments/assets/cf7978dc-c825-4690-b597-967a64bd5b58" />
<img width="4032" height="3024" alt="c_test_50n" src="https://github.com/user-attachments/assets/ee5bfa3f-e909-4c1a-b630-69df62e3c56b" />
<img width="4032" height="3024" alt="c_test_220n" src="https://github.com/user-attachments/assets/eadca295-d143-4cdb-b2d6-27cb6ccbfb0e" />
<img width="4032" height="3024" alt="c_test_100u" src="https://github.com/user-attachments/assets/3a2c35b7-3237-43b3-b3af-34cf597f6ef9" />
<img width="4032" height="3024" alt="l_test_10u" src="https://github.com/user-attachments/assets/c424a9a8-7d25-4672-84d3-f5a845d94e03" />
<img width="4032" height="3024" alt="l_test_100u" src="https://github.com/user-attachments/assets/99fdedb3-da05-45f7-a0a7-25e13a03b185" />

## Additional photos
<img width="4032" height="3024" alt="lcr_2" src="https://github.com/user-attachments/assets/3a485b73-f499-48ef-9328-67a421f1db87" />
<img width="2774" height="2025" alt="lcr_back" src="https://github.com/user-attachments/assets/dad45fe3-3315-4562-9052-68302c408a9b" />

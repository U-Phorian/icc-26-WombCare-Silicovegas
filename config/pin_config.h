#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

// $[CMU]
// [CMU]$

// $[LFXO]
// [LFXO]$

// $[KEYSCAN]
// [KEYSCAN]$

// $[PRS.ASYNCH0]
// [PRS.ASYNCH0]$

// $[PRS.ASYNCH1]
// [PRS.ASYNCH1]$

// $[PRS.ASYNCH2]
// [PRS.ASYNCH2]$

// $[PRS.ASYNCH3]
// [PRS.ASYNCH3]$

// $[PRS.ASYNCH4]
// [PRS.ASYNCH4]$

// $[PRS.ASYNCH5]
// [PRS.ASYNCH5]$

// $[PRS.ASYNCH6]
// [PRS.ASYNCH6]$

// $[PRS.ASYNCH7]
// [PRS.ASYNCH7]$

// $[PRS.ASYNCH8]
// [PRS.ASYNCH8]$

// $[PRS.ASYNCH9]
// [PRS.ASYNCH9]$

// $[PRS.ASYNCH10]
// [PRS.ASYNCH10]$

// $[PRS.ASYNCH11]
// [PRS.ASYNCH11]$

// $[PRS.ASYNCH12]
// [PRS.ASYNCH12]$

// $[PRS.ASYNCH13]
// [PRS.ASYNCH13]$

// $[PRS.ASYNCH14]
// [PRS.ASYNCH14]$

// $[PRS.ASYNCH15]
// [PRS.ASYNCH15]$

// $[PRS.SYNCH0]
// [PRS.SYNCH0]$

// $[PRS.SYNCH1]
// [PRS.SYNCH1]$

// $[PRS.SYNCH2]
// [PRS.SYNCH2]$

// $[PRS.SYNCH3]
// [PRS.SYNCH3]$

// $[GPIO]
// [GPIO]$

// $[TIMER0]
// [TIMER0]$

// $[TIMER1]
// [TIMER1]$

// $[TIMER2]
// [TIMER2]$

// $[TIMER3]
// [TIMER3]$

// $[TIMER4]
// [TIMER4]$

// $[TIMER5]
// [TIMER5]$

// $[TIMER6]
// [TIMER6]$

// $[TIMER7]
// [TIMER7]$

// $[TIMER8]
// [TIMER8]$

// $[TIMER9]
// [TIMER9]$

// $[EUSART1]
// [EUSART1]$

// $[EUSART2]
// [EUSART2]$

// $[EUSART3]
// [EUSART3]$

// $[USART0]
// [USART0]$

// $[USART1]
// [USART1]$

// $[USART2]
// [USART2]$

// $[I2C1]
// [I2C1]$

// $[I2C2]
// [I2C2]$

// $[I2C3]
// [I2C3]$

// $[LCD]
// [LCD]$

// $[LETIMER0]
// [LETIMER0]$

// $[IADC0]
// IADC0 SCAN0POS on PB07
#ifndef IADC0_SCAN0POS_PORT                     
#define IADC0_SCAN0POS_PORT                      SL_GPIO_PORT_B
#endif
#ifndef IADC0_SCAN0POS_PIN                      
#define IADC0_SCAN0POS_PIN                       7
#endif

// IADC0 SCAN1POS on PB08
#ifndef IADC0_SCAN1POS_PORT                     
#define IADC0_SCAN1POS_PORT                      SL_GPIO_PORT_B
#endif
#ifndef IADC0_SCAN1POS_PIN                      
#define IADC0_SCAN1POS_PIN                       8
#endif

// IADC0 SCAN2POS on PD08
#ifndef IADC0_SCAN2POS_PORT                     
#define IADC0_SCAN2POS_PORT                      SL_GPIO_PORT_D
#endif
#ifndef IADC0_SCAN2POS_PIN                      
#define IADC0_SCAN2POS_PIN                       8
#endif

// [IADC0]$

// $[ACMP0]
// [ACMP0]$

// $[ACMP1]
// [ACMP1]$

// $[VDAC0]
// [VDAC0]$

// $[VDAC1]
// [VDAC1]$

// $[PCNT0]
// [PCNT0]$

// $[I2C0]
// [I2C0]$

// $[EUSART0]
// [EUSART0]$

// $[PTI]
// [PTI]$

// $[MODEM]
// [MODEM]$

// $[CUSTOM_PIN_NAME]
#ifndef _PORT                                   
#define _PORT                                    SL_GPIO_PORT_A
#endif
#ifndef _PIN                                    
#define _PIN                                     0
#endif


















#ifndef Maternal Chest ECG output_PORT          
#define Maternal Chest ECG output_PORT           SL_GPIO_PORT_B
#endif
#ifndef Maternal Chest ECG output_PIN           
#define Maternal Chest ECG output_PIN            7
#endif

#ifndef Abdominal ECG output_PORT               
#define Abdominal ECG output_PORT                SL_GPIO_PORT_B
#endif
#ifndef Abdominal ECG output_PIN                
#define Abdominal ECG output_PIN                 8
#endif























#ifndef PVDF Kick Sensor output_PORT            
#define PVDF Kick Sensor output_PORT             SL_GPIO_PORT_D
#endif
#ifndef PVDF Kick Sensor output_PIN             
#define PVDF Kick Sensor output_PIN              8
#endif







// [CUSTOM_PIN_NAME]$


#endif // PIN_CONFIG_H



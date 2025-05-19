/*Limiti minimi e massimi di frequenza e duty cycle*/
#define MIN_FREQ                    (250)         /* Minimum frequency (in mHz) */
#define MAX_FREQ                    (100000)      /* Maximum frequency (in mHz) */
#define MIN_DUTY                    (10)          /* Minimum duty cycle (in %) */
#define MAX_DUTY                    (90)          /* Maximum duty cycle (in %) */

/*Nanosecondi in un secondo (1 secondo = 10^9 nanosecondi)*/
#define NSEC_IN_SEC                 (1000000000)  /* Nanseconds in one second */

/*Tolleranze*/
#define TOLERANCE_FREQUENCY         (5)           /* Tolerance on frequency (per thousand) */
#define TOLERANCE_DUTY              (1)           /* Tolerance on duty cycle (percent) */

#define HUNDRED                     (100)         /* One hundred */
#define THOUSAND                    (1000)        /* One thousand */

/*PIN usati come ingresso e uscita*/
#define INPUT_PIN                   (23)          /* PIN used as input */
#define OUTPUT_PIN                  (53)          /* PIN used as output */


/*Defiizione degli stati della FSM come enumerato di valori*/
enum fsm {
  UNCOUPLED = 0,              /*UNCOUPLED: ancora non ho riconosciuto nemmeno un periodo valido*/
  COUPLING,                   /*COUPLING: ho riconosciuto un singolo periodo valido, sono in attesa del secondo*/
  COUPLED                     /*COUPLED: ho riconosciuto almeno due periodi validi, la frequenza è riconosciuta correttamente*/
};



/*Elenco di variabili statiche (comuni a tutte le funzioni)*/
static uint32_t frequency;    /*Frequenza da riconoscere (mHz) senza tolleranza*/
static uint32_t dutyCycle;    /*Duty cycle da riconoscere (%) senza tolleranza*/
static uint32_t periodMin;    /*Periodo minimo (us) da riconscere periodMin*/
static uint32_t periodMax;    /*Periodo massimo (us) da riconscere periodMax*/
static uint32_t tOnMin;       /*Larghezza d'impulso minima (us) da riconscere TON_min*/
static uint32_t tOnMax;       /*Larghezza d'impulso massima (us) da riconscere TON_max*/


/*Elenco delle funzioni statiche (accessibili solo da questo file .ino)*/
static void printFrequencyRange(bool bInvalid);
static void printDutyCycleRange(bool bInvalid);
static void printConfig(void);
static void configure(void);


/*------------------------------------------------------------------*/
/*Funzione setup                                                    */
/*Configura Arduino Due                                             */
/*Viene chiamata automaticamente da Arduino                         */
/*Configura la porta seriale, aspetta i parametri di configurazione */
/*da seriale, salva la configurazione, configura i due PIN di input */
/*e output, chiude la porta seriale                                 */
/*NOTA: questa funzione non va modificata                           */
/*------------------------------------------------------------------*/
void setup() {
  String tmp;
  unsigned long value;
  int bValid;

  static enum fsm currFSMState = UNCOUPLED;
  static bool prevState = LOW;
  static bool lastTonValid = false;
  static uint32_t lastRisingTime = 0;
  static uint32_t lastFallingTime = 0;

  
  /*Initialize serial and wait for port to open*/
  SerialUSB.begin(9600);
  SerialUSB.setTimeout(60000);
  while (!SerialUSB) {
    ; /*wait for serial port to connect. Needed for native USB*/
  }

  SerialUSB.println("Configuring Frequency Detector...");
  printFrequencyRange(false);
  bValid = 0;
  do {
    tmp = SerialUSB.readStringUntil('\n');
    if (tmp != NULL) {
      value = strtoul(tmp.c_str(), NULL, 10);
      if (value >= MIN_FREQ && value <= MAX_FREQ) {
        frequency = (uint32_t)value;
        bValid = 1;
      }
      else {
        printFrequencyRange(true);
      }
    }
  } while (bValid == 0);
  printDutyCycleRange(false);
  bValid = 0;
  do {
    tmp = SerialUSB.readStringUntil('\n');
    if (tmp != NULL) {
      value = strtoul(tmp.c_str(), NULL, 10);
      if (value >= MIN_DUTY && value <= MAX_DUTY) {
        dutyCycle = (uint32_t)value;
        bValid = 1;
      }
      else {
        printDutyCycleRange(true);
      }
    }
  } while (bValid == 0);

  configure();
  printConfig();
  SerialUSB.end();
}


/*------------------------------------------------------------------*/
/*Funzione loop                                                     */
/*Esegue il main loop di Arduino Due                                */
/*Viene chiamata automaticamente da Arduino                         */
/*Implementa la FSM per riconoscere la frequenza configurata        */
/*NOTA: questa funzione deve essere scritta da voi :)               */
/*------------------------------------------------------------------*/
void loop() {
  static enum fsm currFSMState = UNCOUPLED;
  static bool prevState = LOW;
  static uint32_t lastRisingTime = 0;
  static uint32_t lastFallingTime = 0;
  static bool lastTonValid = false;
  
  bool currState;
  uint32_t currTime;
  uint32_t pulseWidth;
  uint32_t period;
  
  while(1) {
    currTime = micros();
    currState = digitalRead(INPUT_PIN);
    
    // Verifica se lo stato è cambiato
    if (currState != prevState) {
      // Fronte di salita (LOW a HIGH)
      if (currState == HIGH) {
        if (lastRisingTime > 0) {
          period = currTime - lastRisingTime;
          
          // Verifica se il periodo è valido
          if ((period >= periodMin) && (period <= periodMax)) {
            switch (currFSMState) {
              case UNCOUPLED:
                if (lastTonValid) {
                  currFSMState = COUPLING;
                }
                break;
              
              case COUPLING:
                if (lastTonValid) {
                  currFSMState = COUPLED;
                  digitalWrite(OUTPUT_PIN, HIGH);
                } else {
                  currFSMState = UNCOUPLED;
                }
                break;
              
              case COUPLED:
                // Mantiene lo stato COUPLED
                break;
            }
          } else {
            // Periodo non valido
            if (currFSMState == COUPLED) {
              digitalWrite(OUTPUT_PIN, LOW);
            }
            currFSMState = UNCOUPLED;
            lastTonValid = false;
          }
        }
        lastRisingTime = currTime;
      }
      // Fronte di discesa (HIGH a LOW)
      else {
        if (lastRisingTime > 0) {
          pulseWidth = currTime - lastRisingTime;
          
          // Verifica se la larghezza d'impulso è valida
          if ((pulseWidth >= tOnMin) && (pulseWidth <= tOnMax)) {
            lastTonValid = true;
          } else {
            // Larghezza d'impulso non valida
            lastTonValid = false;
            if (currFSMState == COUPLED) {
              digitalWrite(OUTPUT_PIN, LOW);
            }
            currFSMState = UNCOUPLED;
          }
        }
        lastFallingTime = currTime;
      }
      
      // Aggiorna lo stato precedente
      prevState = currState;
    } else {
      // Verifica timeout se lo stato non è cambiato
      if (currState == HIGH) {
        // Se in stato HIGH da troppo tempo (TON troppo lungo)
        if ((currTime - lastRisingTime) > tOnMax) {
          lastTonValid = false;
          if (currFSMState == COUPLED) {
            digitalWrite(OUTPUT_PIN, LOW);
          }
          currFSMState = UNCOUPLED;
        }
      } else {
        // Se in stato LOW da troppo tempo (periodo troppo lungo)
        if (lastFallingTime > 0 && (currTime - lastFallingTime) > (periodMax - tOnMax)) {
          if (currFSMState == COUPLED) {
            digitalWrite(OUTPUT_PIN, LOW);
          }
          currFSMState = UNCOUPLED;
        }
      }
    }
  }
}





/*------------------------------------------------------------------*/
/*Funzione printFrequencyRange                                      */
/*Stampa il range ammesso per la prequenza                          */
/*NOTA: questa funzione non va modificata                           */
/*------------------------------------------------------------------*/
static void printFrequencyRange(bool bInvalid) {
  SerialUSB.print("  ");
  if (bInvalid != false) {
    SerialUSB.print("Invalid value. ");
  }
  SerialUSB.print("Provide frequency (mHz) [");
  SerialUSB.print(MIN_FREQ, DEC);
  SerialUSB.print("-");
  SerialUSB.print(MAX_FREQ, DEC);
  SerialUSB.println("]");
}


/*------------------------------------------------------------------*/
/*Funzione printDutyCycleRange                                      */
/*Stampa il range ammesso per il duty cycle                         */
/*NOTA: questa funzione non va modificata                           */
/*------------------------------------------------------------------*/
static void printDutyCycleRange(bool bInvalid) {
  SerialUSB.print("  ");
  if (bInvalid != false) {
    SerialUSB.print("Invalid value. ");
  }
  SerialUSB.print("Provide duty cycle (%) [");
  SerialUSB.print(MIN_DUTY, DEC);
  SerialUSB.print("-");
  SerialUSB.print(MAX_DUTY, DEC);
  SerialUSB.println("]");
}


/*------------------------------------------------------------------*/
/*Funzione printConfig                                              */
/*Stampa la configurazione ricevuta (frequenza e duty cycle)        */
/*NOTA: questa funzione non va modificata                           */
/*------------------------------------------------------------------*/
static void printConfig(void) {
  SerialUSB.println();
  SerialUSB.println("Frequency Detector configuration");
  SerialUSB.print("  Period: [");
  SerialUSB.print(periodMin, DEC);
  SerialUSB.print(" - ");
  SerialUSB.print(periodMax, DEC);
  SerialUSB.println("] us");
  SerialUSB.print("  T_ON: [");
  SerialUSB.print(tOnMin, DEC);
  SerialUSB.print(" - ");
  SerialUSB.print(tOnMax, DEC);
  SerialUSB.println("] us");
  SerialUSB.print("  Input PIN: D");
  SerialUSB.println(INPUT_PIN, DEC);
  SerialUSB.print("  Output PIN: D");
  SerialUSB.println(OUTPUT_PIN, DEC);
}


/*------------------------------------------------------------------*/
/*Funzione configure                                                */
/*Calcola i valori di periodo minimo e massimo (periodMin e periodMax) in   */
/*microsecondi (us) e i valori di larghezz d'impulso minima e       */
/*massima (TON_min e TON_max) in microsecondi (us)                  */
/*Configura i due PIN INPUT_PIN e OUTPUT_PIN come ingresso e uscita */
/*NOTA: questa funzione deve essere scritta da voi :)               */
/*------------------------------------------------------------------*/
static void configure(void) {
  uint32_t freq;
  
  /*Calcoliamo il periodo minimo e massimo*/
  /*periodMin = 1 / f_max*/
  /*periodMax = 1 / f_min*/
  /*NOTA: la frequenza è in mHz (quindi andrebbe divisa per 1000), il periodo lo vogliamo in microsecondi (quindi lo vorremmo moltiplicare per 10^6)*/
  /*Per minimizzare l'errore numerico possiamo fare:*/
  /*    T(us) = 10^9(ns) / f(mHz)     */
  freq = (((THOUSAND + TOLERANCE_FREQUENCY) * frequency) + (THOUSAND - 1)) / THOUSAND;
  periodMin = (NSEC_IN_SEC) / freq;
  freq = ((THOUSAND - TOLERANCE_FREQUENCY) * frequency) / THOUSAND;
  periodMax = (NSEC_IN_SEC + (freq - 1)) / freq;

  /*Calcoliamo la larghezza d'impulso minima e massima*/
  /*TON_min = periodMin * d_min / 100*/
  /*TON_max = periodMax * d_max / 100*/
  tOnMin = ((dutyCycle - TOLERANCE_DUTY) * periodMin) / HUNDRED;
  tOnMax = (((dutyCycle + TOLERANCE_DUTY) * periodMax) + (HUNDRED - 1)) / HUNDRED;

  /*Configuriamo il piedino INPUT_PIN come ingresso e il piedino OUTPUT_PIN come uscita*/
  pinMode(INPUT_PIN, INPUT);
  pinMode(OUTPUT_PIN, OUTPUT);
}

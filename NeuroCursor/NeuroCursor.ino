#include <Mouse.h>
#include <Keyboard.h>
// Настройки
  // Вывод в BiTronics, иначе в Serial
  #define BITRONICS 1
  // Подключение дополнительных библиотек и переназначение пинов для Стрелы от Амперки
  // Если используется иная плата, то чуть ниже нужно добавить дефайны недостающих пинов
  #define STRELA 1
  // Использовать мышь?
  #define MOUSE 1
  // Задержка мыши. Больше — производительнее, меньше — плавнее
  #define MOUSEDELAY 1
  // Левша
  #define LEVSHA 0
  // Пины
  #define CALIBBTN 1
  #define LOCKBTN 2

// Для стрелы
#if(STRELA)
  // Дополнительные библиотеки
  #include <Strela.h>
  #include <Wire.h>
  // Переназначение функций
  #define digitalWrite(a, b) uDigitalWrite(a, b)
  #define digitalRead(a) uDigitalRead(a)
  #define analogWrite(a, b) uAnalogWrite(a, b)
  #define analogRead(a) uAnalogRead(a)
  #define pinMode(a, b) uPinMode(a, b)
  // Пины
  #define LED L1
    // Бицепс
    #define EMG1 P5
    // Плечо
    #define EMG2 P8
  #define CALIBBTN S1
  #define LOCKBTN S2
#endif

// Магические числа
  // Кол-во значений для анализа триггера
  #define ARRSIZETRIG 150
  // Кол-во значений для анализа частоты
  #define ARRSIZEFREQ 100
  // Кол-во попугаев, на которое должна напрячься мышца, чтобы сработал триггер
  #define THRESHOLD 120
  #define ELTHRESHOLD 90
  // Кол-во попугаев, на которое должна увеличиться частота локтя, чтобы сработал триггер
  #define ELTHRESHOLDFREQ 20
  // Кол-во попугаев, на которое должна увеличиться частота плеча, чтобы сработал триггер
  #define SHTHRESHOLDFREQ 300 //80

// Структура для флагов
struct
{
  uint8_t elbow : 1;
  uint8_t lrud : 1;
  uint8_t click : 1;
  uint8_t lock : 1;
  uint8_t prevLockBtn : 1;
} bools;

struct diffAvr
{
  uint8_t diff, avr;           // Среднее, разница, максимум
};

// Структура данных для каждого датчика
struct values4emg
{
  uint8_t pin;                  // Пин
  uint8_t val[250];             // Сырые данные
  uint8_t midlFiltr;            // Вспомогательный фильтр
  uint8_t valFiltr[250];        // Фильтр
  uint8_t valAvr[ARRSIZEFREQ];  // Среднее
  diffAvr da, daFiltr;          // Среднее среди ARRSIZE значений
  uint8_t maxDiff;              // Максимальный размах
  uint16_t freq;                // Частота сигнала
  uint16_t maxFreq;             // Максимальная частота сигнала
  uint16_t threshold;           // Порог силы
  uint16_t thresholdFreq;       // Порог частоты
  uint8_t trig : 1;             // Триггер
  uint8_t prevTrig : 1;         // Предыдущий триггер
  uint8_t trigFiltr : 1;        // Триггер
  uint8_t prevTrigFiltr : 1;    // Предыдущий триггер
} emg1, emg2;

// Калибровка порогов срабатывания
void calibrate()
{
  emg1.threshold = emg1.maxDiff + THRESHOLD;
  emg2.threshold = emg2.maxDiff + ELTHRESHOLD;
  emg1.thresholdFreq = emg1.maxFreq + ELTHRESHOLDFREQ;
  emg2.thresholdFreq = emg2.maxFreq + SHTHRESHOLDFREQ;
}

diffAvr calcAvr(uint8_t val[], uint8_t arrSize)
{
  // Вычисляем разницу
  uint8_t max = 0;
  uint8_t min = 255;
  for (uint16_t k = 0; k < arrSize; k++)
  {
    if (val[k] > max)
      max = val[k];
    if (val[k] < min)
      min = val[k];
  }
  uint8_t diff = max - min;

  // Вычисляем среднее
  uint8_t avr = min + (diff / 2);

  // Упаковка и возврат
  diffAvr result;
  result.diff = diff;
  result.avr = avr;
  return result;
}

void calc(values4emg *emg)
{
  // Сдвигаем старые значения и вычисляем частоту сигнала
  emg->freq = 0;
  for(uint8_t i = 249; i > 0; i--)
  {
    emg->val[i] = emg->val[i - 1];
    emg->valFiltr[i] = emg->valFiltr[i - 1];
  }
  for(uint8_t i = 0; i < ARRSIZEFREQ; i++)
  {
    emg->valAvr[i] = emg->valAvr[i - 1];
  }
  for(uint8_t i = 0; i < ARRSIZEFREQ; i++)
  {
    emg->freq +=  (emg->val[i] <= emg->valAvr[i] && emg->val[i-1] >= emg->valAvr[i-1]) ||
                  (emg->val[i] >= emg->valAvr[i] && emg->val[i-1] <= emg->valAvr[i-1]);
  }
  // Увеличиваем значение частоты, чтобы можно было различить на графике
  emg->freq *= 20;

  if(emg->freq > emg->maxFreq) emg->maxFreq = emg->freq;
  else if(millis() % 10 == 0) emg->maxFreq -= 1;

  // Считываем значение с датчика и подгоняем под формат BiTronics
  emg->val[0] = analogRead(emg->pin) >> 2;

  // Фильтруем пульс
  emg->midlFiltr =  emg->trig ?
                    emg->val[0] :
                    (63 * emg->midlFiltr + emg->val[0]) >> 6;
  // Вычитаем пульс
  //emg->valFiltr[0] = (7 * emg->valFiltr[0] + emg->midlFiltr) >> 3;
  emg->valFiltr[0] = emg->val[0] - emg->da.avr + 128;

  // Вычисляем разницу и среднее
  emg->da = calcAvr(emg->val, ARRSIZEFREQ);
  emg->daFiltr = calcAvr(emg->valFiltr, ARRSIZETRIG);
  emg->valAvr[0] = emg->da.avr;

  if(emg->daFiltr.diff > emg->maxDiff) emg->maxDiff = emg->daFiltr.diff;
  else if(millis() % 10 == 0) emg->maxDiff -= 1;

  // Разница преодолела порог?
  emg->prevTrig = emg->trig;
  emg->trig = emg->daFiltr.diff > emg->threshold;
}

// Отправляем данные в Serial или BiTronics
void sendData()
{
  #if(BITRONICS)
  Serial.write("A0");
  Serial.write(map(millis() % 2 ? emg1.daFiltr.diff : emg1.threshold, 0, 255, 0, 255));  
  Serial.write("A2");
  Serial.write(map(millis() % 2 ? emg2.daFiltr.diff : emg2.threshold, 0, 255, 0, 255));
  Serial.write("A1");
  Serial.write(map(millis() % 2 ? emg1.freq : emg1.thresholdFreq, 0, 1024, 0, 255));
  Serial.write("A3");
  Serial.write(map(millis() % 2 ? emg1.val[0] : emg1.valAvr[0], 0, 255, 0, 255));
  #else
  Serial.print(emg1.val[0]);
  Serial.print(",");
  Serial.print(emg1.freq);
  Serial.print(",");
  Serial.println(emg1.da.avr);
  #endif
}

void makeAMove()
{
  // Таймеры движения и клика
  static uint32_t udtimer = 0;
  static uint32_t clickTimer = 0;

  if(bools.elbow)
  {
    if(emg1.prevTrig == 1) emg1.trig = 1;
    if(emg1.daFiltr.diff < emg1.threshold - 20) emg1.trig = 0;
  }

  // Начинаем отсчёт
  if(emg1.prevTrig == 0 && emg1.trig == 1) udtimer = millis();
  if(emg2.prevTrig == 0 && emg2.trig == 1) clickTimer = millis();

  // Если бицепс напряжён
  if(emg1.trig == 1)
  {
    if(millis() - udtimer > 600)
    {
    #if(MOUSE)
    // Определяем ускорение курсора
    if(!bools.lock && millis() % 
        ((millis() - udtimer > 1500) ?
        ((millis() - udtimer > 3000) ? 1 : 3) :
        5) == 0)
      // Двигаем курсор в нужном направлении
      Mouse.move( bools.lrud ? (bools.elbow ? 1 : -1) * (LEVSHA ? -1 : 1) * MOUSEDELAY : 0,
                  bools.lrud ? 0 : (bools.elbow ? 1 : -1) * MOUSEDELAY, 0);
      #endif
    }
    else
    // Первые 600 миллисекунд определяем направление
    {
      // Если локоть разогнут на 180 градусов, то частота сигнала резко повышается
      bools.elbow |= emg1.freq > emg1.thresholdFreq;
    }
  }

  // Если плечо напряжнно
  if(emg2.trig == 1)
  {
    if((millis() - udtimer > 500) && (millis() - udtimer < 600))
    {
      bools.lrud = 1;
    }
  }

  // Сброс флагов по заднему фронту
  if(emg1.prevTrig == 1 && emg1.trig == 0) { bools.elbow = 0; bools.lrud = 0; } 

  // Распознаём и делаем клик
  if(emg2.prevTrig == 1 && emg2.trig == 0)
  {
    #if(MOUSE)
    if(millis() - clickTimer < 700 && !(millis() - udtimer > 700) && !bools.lock) Mouse.click(MOUSE_LEFT);
    #endif
  }
}

void setup()
{
  pinMode(EMG1, INPUT);
  pinMode(EMG2, INPUT);
  pinMode(CALIBBTN, INPUT);
  pinMode(LOCKBTN, INPUT);
  pinMode(LED, OUTPUT);

  emg1.pin = EMG1;
  emg2.pin = EMG2;

  Serial.begin(115200);

  // Инициализация HID
  #if(MOUSE)
  Mouse.begin();
  #endif
  //Keyboard.begin();

  digitalWrite(LED, LOW);

  // Первоначальная калибровка
  while(millis() < 5000) { calc(&emg1); calc(&emg2); }
  calibrate();
  while(millis() < 10000) { calc(&emg1); calc(&emg2); }
  calibrate();
  // bools.lock = 0;
}

void loop()
{
  // Таймеры
  static uint32_t timer1 = 0;
  static uint32_t timer2 = 0;
  static uint32_t timer3 = 0;
  static uint32_t timer4 = 0;

  // Каждую миллисекунду
  if(millis() - timer1 >= 1)
  {
    timer1 = millis();

    // Считываем, вычисляем
    calc(&emg1);
    calc(&emg2);
  }
  
  if(millis() - timer2 >= 1000)
  {
    timer2 = millis();
    
    // Отправляем
    sendData();
  }
  
  if(millis() - timer3 >= MOUSEDELAY)
  {
    timer3 = millis();
    
    // Двигаем
    makeAMove();
  }
  
  if(millis() - timer4 >= 200)
  {
    timer4 = millis();
    
    // Калибровка по нажатию кнопки
    if(digitalRead(CALIBBTN)) calibrate();

    // T-триггер кнопки блокировки движения курсора
    bools.lock ^= (digitalRead(LOCKBTN) && !bools.prevLockBtn);
    bools.prevLockBtn = digitalRead(LOCKBTN);
  }
}

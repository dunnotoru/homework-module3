# Результаты обработки сигналов
## SIGINT

**Ожидается**: Программа должна завершаться после третьего сигнала SIGINT.  

[term 1] \$ make run &
Counter: 1  
Counter: 2  
Counter: 3  
Counter: 4  
Counter: 5  
Counter: 6  
THE END  

[term 2] \$ pkill -SIGINT app  
[term 2] \$ pkill -SIGINT app  
[term 2] \$ pkill -SIGINT app  

**Результат:** В результате обработки сигнала, программа завершается после 3-го сигнала SIGINT.  
В файл записывается сообщение о получении сигнала.

## SIGQUIT

**Ожидается:** Программа должна логировать получение SIGQUIT в файл, но не завершаться.  

[term 1] \$ make run &
Counter: 1  
Counter: 2  
Counter: 3  
Counter: 4  
Counter: 5  
Counter: 6  
Counter: 7  
Counter: 8  
Counter: 9  
Counter: 10  
Counter: 11  
Counter: 12  
Counter: 13  
Counter: 14  
Counter: 15
THE END

[term 2] \$ pkill -SIGQUIT app  
[term 2] \$ pkill -SIGQUIT app  
[term 2] \$ pkill -SIGQUIT app  
[term 2] \$ pkill -SIGQUIT app  
[term 2] \$ pkill -SIGQUIT app  
[term 2] \$ pkill -SIGINT app  
[term 2] \$ pkill -SIGINT app  
[term 2] \$ pkill -SIGINT app  

**Результат:** Программа обрабатывает SIGQUIT и записывает сообщение в файл, но не завершается после третьего.  
Для остановки программы пришлось отправить SIGINT трижды.

## SIGABRT

**Ожидается:** завершение программы.  

\$ make run >/dev/null &
[1] 331819
\$ pkill -SIGABRT app  
make: *** [Makefile:8: run] Аварийный останов                                                                                                                        
[1]  + 331819 exit 2     make run > /dev/null  

**Результат:** Программа завершается при получении SIGABRT

## SIGKILL

**Ожидается:** Программа должна быть убита!!! 

\$ make run >/dev/null &
[1] 332474  
\$ • pkill -SIGKILL app  
make: *** [Makefile:8: run] Убито                   
[1]  + 332474 exit 2     make run > /dev/null

**Результат:** Программа убита...

## SIGTERM

**Ожидается:** Программа должна быть завершена.

\$ make run >/dev/null &
[1] 336383
\$ pkill -SIGTERM app   
make: *** [Makefile:8: run] Завершено                   
[1]  + 336383 exit 2     make run > /dev/null  

**Результат:** Программа завершена.

## SIGTSTP

**Ожидается:** Приостановка процесса.

[term 1] \$ make run &  
[1] 343635  
./build/app                                         
[term 1] \$ Counter: 1  
Counter: 2  
[term 1] \$ Counter: 3  
Counter: 4  
Counter: 5  
Counter: 6  
Counter: 7  
Counter: 8  
Counter: 9  
Counter: 10  
Counter: 11  
Counter: 12  
Counter: 13  
Counter: 14  

[term 2] \$ pkill -SIGTSTP app

[term 2] \$ ps -p 343635 -o stat,cmd
STAT CMD
SN   make run

**Результат:** Вывод остановился. Процесс находится в состоянии прерываемого сна (S)

## SIGSTOP

**Ожидается:** Приостановка процесса.

\$ make run &               
[1] 344365  
./build/app                           
\$ Counter: 1  
Counter: 2  
Counter: 3  
Counter: 4  
Counter: 5  
Counter: 6  
Counter: 7  
Counter: 8  

[term 2] \$ pkill -SIGSTOP app  

[term 2] \$ ps -p 344365 -o stat,cmd    
STAT CMD  
SN   make run   

**Результат:** Тоже... приостановка процесса? В отличие от SIGTSTP, этот сигнал нельзя перехватить программно.  

## SIGCONT 

**Ожидается:** Остановленный процесс при получении должен продолжить выполнение.  

\$ make run   
./build/app  
Counter: 1  
Counter: 2  
Counter: 3  
^Z  
[1]  + 345433 suspended  make run  

\$ pkill -SIGCONT app  
Counter: 4  
\$ Counter: 5  
Counter: 6  
Counter: 7  

**Результат:** Процесс остановленный с помощью SIGTSTP (Ctrl+Z) продолжился.  
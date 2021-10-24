Nume: Orzata Miruna-Narcisa,
Grupă: 331CA

# Tema 4 SO (Planificator de thread-uri)

Organizare
-
* Ideea: Implementarea unui **planificator de thread-uri in user-space** sub forma unei biblioteci partajate. 
* Solutia se bazeaza pe dezvoltarea unui program in C pe sistemele **Unix si Windows**, care simuleaza un **scheduler de procese preemptiv**, într-un sistem uniprocesor, si utilizează un algoritm de planificare **Round Robin cu priorități**.


Implementare
-
* Biblioteca dinamica ofera o interfata comuna tuturor thread-urilor ce o utilizeaza si urmeaza a fi planificate. Astfel, functiile exportate de biblioteca sunt următoarele:
    * **so_init(cuantă, io)** - ce are rolul de a initializa structura interna a planificatorului.
    * **so_end()** - va elibera resursele planificatorului și așteaptă terminarea tuturor threadurilor înainte de părăsirea sistemului.
    * **so_fork(handler, prioritate)** - va crea un nou thread in kernel-space si va adauga toate informatiile necesare despre acesta in coada de prioritati a planificatorului. De indata ce thread-ul din kernel-space a pornit, acesta anunta thread-ul parinte ca este gata pentru planficare(este gata pentru a rula handler-ul primit ca parametru), iar thread-ul parinte se trezeste in momentul urmator si va incepe procesul de planificare, alegand cel mai potrivit thread pentru a rula pe procesor. In tot acest timp, thread-urile nou create, ce nu au fost planificate, vor astepta pana cand planificatorul le va alege intr-un pas ulterior.
    * **so_exec()** - va simula executia unei instructiuni pe procesor, ceea ce va duce la scaderea cuantei pentru thread-ul ce o apeleaza.
    * **so_wait(event/io)** - va marca thread-ul curent ca fiind blocat pe un eveniment I/O si nu ii va permite acestuia sa fie planificat pe procesor decat dupa ce va primi un semnal de la alt thread pe acelasi eveniment.
    * **so_signal(event/io)** - are rolul de a trezi toate thread-urile ce asteptau pe evenimentul mentionat ca parametru.
* Functia cea mai complexa, **schedule**, are la baza o coada de prioritati cu thread-uri, ce va retine intotdeauna cel mai bun thread in varf, exceptie facand cazurile in care thread-urile din varf asteapta dupa un eveniment I/O.
* Planificatorul va preempta thread-ul curent ce ruleaza pe procesor in urmatoarele cazuri:
    * un thread cu o prioritate mai mare intră în sistem.
    * un thread cu o prioritate mai mare este semnalat printr-o operație de tipul signal.
    * thread-ului curent i-a expirat cuanta de timp.
    * thread-ul curent așteaptă la un eveniment in urma operatiei wait.
    * thread-ul curent nu mai are instrucțiuni de executat.
* Un caz special intalnit in timpul algoritmului de planificare este atunci cand in sistem se afla mai multe thread-uri cu aceeasi prioritate, iar acestea trebuie sa fie planificate alternativ prin Round Robin, in momentul in care li s-a terminat cuanta. In acest sens, daca task-ului curent i s-a terminat cuanta si nu a intrat in sistem un task cu o prioritate mai mare, atunci voi alege urmatorul task cu aceeasi prioritate sau cu o prioritate mai mica, daca nu gasesc aceeasi prioritate. Astfel, scot din coada task-ul curent si il adaug din nou pentru a avea la rand urmatorul task cu aceeasi prioritate.
* Alte detalii:
    * Blocarea thread-ului curent s-a bazat pe folosirea constructiei mutex-variabila_de_conditie-mutex, prin intermediul careia thread-ul apeleaza **pthread_mutex_lock** / **EnterCriticalSection**, asteapta pana cand un predicat devine adevarat prin functia **pthread_cond_wait** / **SleepConditionVariableCS**, iar apoi elibereaza lock-ul cand conditia se indeplineste prin **pthread_mutex_unlock** / **LeaveCriticalSection**.
    * Similar, semnalarea comenzii de rulare pe procesor catre un thread se realizeaza prin setarea unei variabile cu id-ul thread-ului ales si apelarea functiei **pthread_cond_broadcast** / **WakeAllConditionVariable**, tot in cadrul unei regiuni critice.
    * Dupa efectuarea fiecarei functii puse la dispozitie de biblioteca, cuanta thread-ului apelant se va decrementa si se va planifica cel mai bun thread.

Cum se compilează și cum se rulează?
-
* In urma comenzii make / nmake, se genereaza biblioteca dinamica libscheduler.so / libscheduler.dll.
* Comanda "make -f Makefile.checker" / "nmake /F NMakefile.checker" va compila sursele folosite de checker.
* Fiecare test se ruleaza separat astfel: "./_test/run_test init", urmat de "./_test/run_test <nr_test>"
* Toate testele se ruleaza prin: "./run_all.sh".

Resurse utilizate
-
* https://ocw.cs.pub.ro/courses/so/laboratoare/laborator-08
* https://ocw.cs.pub.ro/courses/so/laboratoare/laborator-09
* https://docs.microsoft.com/en-us/windows/win32/sync/condition-variables?redirectedfrom=MSDN
* https://docs.microsoft.com/en-us/windows/win32/sync/using-condition-variables

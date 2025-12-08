#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "process.h"
#include "queue.h"
#include "scheduler.h"

int num_algorithms() {
  return sizeof(algorithmsNames) / sizeof(char *);
}

int num_modalities() {
  return sizeof(modalitiesNames) / sizeof(char *);
}

size_t initFromCSVFile(char* filename, Process** procTable){
    FILE* f = fopen(filename,"r");
    
    size_t procTableSize = 10;
    
    *procTable = malloc(procTableSize * sizeof(Process));
    Process * _procTable = *procTable;

    if(f == NULL){
      perror("initFromCSVFile():::Error Opening File:::");   
      exit(1);             
    }

    char* line = NULL;
    size_t buffer_size = 0;
    size_t nprocs= 0;
    while( getline(&line,&buffer_size,f)!=-1){
        if(line != NULL){
            Process p = initProcessFromTokens(line,";");

            if (nprocs==procTableSize-1){
                procTableSize=procTableSize+procTableSize;
                _procTable=realloc(_procTable, procTableSize * sizeof(Process));
            }

            _procTable[nprocs]=p;

            nprocs++;
        }
    }
   free(line);
   fclose(f);
   return nprocs;
}

size_t getTotalCPU(Process *procTable, size_t nprocs){
    size_t total=0;
    for (int p=0; p<nprocs; p++ ){
        total += (size_t) procTable[p].burst;
    }
    return total;
}

int getCurrentBurst(Process* proc, int current_time){
    int burst = 0;
    for(int t=0; t<current_time; t++){
        if(proc->lifecycle[t] == Running){
            burst++;
        }
    }
    return burst;
}

int run_dispatcher(Process *procTable, size_t nprocs, int algorithm, int modality, int quantum){

    Process * _proclist;

    qsort(procTable,nprocs,sizeof(Process),compareArrival);

    init_queue();
    size_t duration = getTotalCPU(procTable, nprocs) +100;

    for (int p=0; p<nprocs; p++ ){
        procTable[p].lifecycle = malloc( duration * sizeof(int));
        for(int t=0; t<duration; t++){
            procTable[p].lifecycle[t]=-1;
        }
        procTable[p].waiting_time = 0;
        procTable[p].return_time = 0;
        procTable[p].response_time = -1;
        procTable[p].completed = false;
    }

    Process* current_process = NULL;
    int quantum_counter = 0;
    int next_arrival_idx = 0;
    int actual_duration = 0;
                           //for per simular ticks de cpu i while per encolar
    for (int t = 0; t < (int)duration; t++){
                                      
        while (next_arrival_idx < (int)nprocs && procTable[next_arrival_idx].arrive_time == t){
            enqueue(&procTable[next_arrival_idx]);
            next_arrival_idx++;
        }
    
        if (algorithm == FCFS){  //utilitzem els ifs segons quin tipus sigui
            if (current_process == NULL && get_queue_size() > 0){
                current_process = dequeue();
            }
        }

        else if (algorithm == SJF){ 
            if (modality == NONPREEMPTIVE && get_queue_size() > 0){ 
                if (current_process == NULL && get_queue_size() > 0){ //Si no hi ha procés executant-se (xq no és apropiatiu)
                    size_t queue_size = get_queue_size();
                    Process* shortest = NULL; //per a guardar el procés amb el burst + petit. 
                    int min_burst = 9999;
                
                    Process** temp_queue = malloc(queue_size * sizeof(Process*));
                    for (size_t i = 0; i < queue_size; i++){
                        temp_queue[i] = dequeue();     //DESAPILEM de la cua per apilar a un array i anar buscant el burst + petit
                        if (temp_queue[i]->burst < min_burst){
                            min_burst = temp_queue[i]->burst;
                            shortest = temp_queue[i];
                        }
                    }
                
                    current_process = shortest; //el marquem com a current per executar-lo
                
                    for (size_t i = 0; i < queue_size; i++){
                        if (temp_queue[i] != shortest){
                            enqueue(temp_queue[i]); //fem enqueue de tots els processos que hem tret menys el shortest
                        }
                    }
                
                    free(temp_queue); //alliberem l'array temporal per recorrer, no el necessitem per res 
                }
            }
            else { //si entra aqui -> apropiatiu
                if (get_queue_size() > 0){
                    size_t queue_size = get_queue_size();
                    Process* shortest = NULL;
                    int min_remaining = 9999;//inicialitzem amb valor molt alt x trobar el min.
            
                    Process** temp_queue = malloc(queue_size * sizeof(Process*));
                    for (size_t i = 0; i < queue_size; i++){
                        temp_queue[i] = dequeue();
                        int remaining = temp_queue[i]->burst - getCurrentBurst(temp_queue[i], t);
                        if (remaining < min_remaining){
                            min_remaining = remaining;
                            shortest = temp_queue[i]; //part imp., guardem el procés amb menys temps restant
                        }
                    }

                    if (current_process != NULL){ //(part principal per la part apropiativa)
                        int current_remaining = current_process->burst - getCurrentBurst(current_process, t);
                        // Si el procés de la cua té menys temps restant, EXPROPIEM
                        if (min_remaining < current_remaining){
                            enqueue(current_process); // Expropiem
                            current_process = shortest;
                        } else {
                            enqueue(shortest); //l'actual segueix
                        }
                    } else {
                        // Si no hi ha cap procés executant-se --> agafem el de menor temps restant
                        current_process = shortest;
                    }
            
            
                    for (size_t i = 0; i < queue_size; i++){
                        if (temp_queue[i] != shortest){
                            enqueue(temp_queue[i]);
                        }
                    }   
            
                    free(temp_queue);
                }
            }
        }
    

        else if (algorithm == PRIORITIES && modality == NONPREEMPTIVE){
            if (current_process == NULL && get_queue_size() > 0){ //si la cpu està lliure... (xq no és apropiatiu)
                size_t queue_size = get_queue_size();
                Process* highest = NULL;
                int min_priority = 9999; //perquè busquem la més petita (+petita = +importancia)
                
                Process** temp_queue = malloc(queue_size * sizeof(Process*));
                for (size_t i = 0; i < queue_size; i++){
                    temp_queue[i] = dequeue(); //la cua de preparats es desencua i va a array temporal
                    if (temp_queue[i]->priority < min_priority){  
                        min_priority = temp_queue[i]->priority; 
                        highest = temp_queue[i];
                    }
                }
                
                current_process = highest;
                
                for (size_t i = 0; i < queue_size; i++){
                    if (temp_queue[i] != highest){
                        enqueue(temp_queue[i]);
                    }
                }
                
                free(temp_queue);
            }
        }

        else if (algorithm == RR){
                                   //aqui a més, tenim en compte si el quantum ha acabat            
            if (current_process == NULL || quantum_counter >= quantum){
                
                if (current_process != NULL){
                    int burst_done = getCurrentBurst(current_process, t);
                    if (burst_done < current_process->burst){
                        enqueue(current_process); //es retorna a la cola si el procés encara no ha acabat 
                    }
                }
                
                if (get_queue_size() > 0){ 
                    current_process = dequeue(); 
                    quantum_counter = 0;
                } else {
                    current_process = NULL;
                    quantum_counter = 0;
                }
            }
        }
        
        if (current_process != NULL){
            current_process->lifecycle[t] = Running; //ho necessitem per fer el diagrama (marcar les E en cada tick de cpu)
            
            if (current_process->response_time == -1){ //si és la primera veg que s'executa -> necessitem calcular el temps de resposta per fer les metriques
                current_process->response_time = t - current_process->arrive_time; 
            }
            
            if (algorithm == RR){ //(especialment pel RR, xq per cada tick incremento els quantums utilitzats)
                quantum_counter++;
            }
                             //aquesta funció ens dona les ràfegues del procés 
            int burst_done = getCurrentBurst(current_process, t + 1);
            
            if (burst_done >= current_process->burst){  //si el procés ha acabat...
                current_process->completed = true; 
                current_process->return_time = t + 1 - current_process->arrive_time;
                current_process = NULL;
                quantum_counter = 0;
            }
        }

        bool all_done = true; //revisem si tots els processos han acabat 
        for (int p = 0; p < nprocs; p++){
            if (!procTable[p].completed){
                all_done = false;
                break;
            }
        }
        if (all_done){
            actual_duration = t + 1;
            break;
        }
    }
   

    for (int p = 0; p < nprocs; p++){ //per calcular les metriques
        procTable[p].waiting_time = procTable[p].return_time - procTable[p].burst;
    }

    printSimulation(nprocs,procTable,actual_duration);
    printMetrics(actual_duration, nprocs, procTable);

    for (int p=0; p<nprocs; p++ ){
        destroyProcess(procTable[p]);
    }

    cleanQueue();
    return EXIT_SUCCESS;
}

void printSimulation(size_t nprocs, Process *procTable, size_t duration){

    printf("%14s","== SIMULATION ");
    for (int t=0; t<duration; t++ ){
        printf("%5s","=====");
    }
    printf("\n");

    printf ("|%4s", "name");
    for(int t=0; t<duration; t++){
        printf ("|%2d", t);
    }
    printf ("|\n");

    for (int p=0; p<nprocs; p++ ){
        Process current = procTable[p];
            printf ("|%4s", current.name);
            for(int t=0; t<duration; t++){
                printf("|%2s",  (current.lifecycle[t]==Running ? "E" : 
                        current.lifecycle[t]==Bloqued ? "B" :   
                        current.lifecycle[t]==Finished ? "F" : " "));
            }
            printf ("|\n");
        
    }


}

void printMetrics(size_t simulationCPUTime, size_t nprocs, Process *procTable ){

    printf("%-14s","== METRICS ");
    for (int t=0; t<simulationCPUTime+1; t++ ){
        printf("%5s","=====");
    }
    printf("\n");

    printf("= Duration: %ld\n", simulationCPUTime );
    printf("= Processes: %ld\n", nprocs );

    size_t baselineCPUTime = getTotalCPU(procTable, nprocs);
    double throughput = (double) nprocs / (double) simulationCPUTime;
    double cpu_usage = (double) simulationCPUTime / (double) baselineCPUTime;

    printf("= CPU (Usage): %lf\n", cpu_usage*100 );
    printf("= Throughput: %lf\n", throughput*100 );

    double averageWaitingTime = 0;
    double averageResponseTime = 0;
    double averageReturnTime = 0;
    double averageReturnTimeN = 0;

    for (int p=0; p<nprocs; p++ ){
            averageWaitingTime += procTable[p].waiting_time;
            averageResponseTime += procTable[p].response_time;
            averageReturnTime += procTable[p].return_time;
            averageReturnTimeN += procTable[p].return_time / (double) procTable[p].burst;
    }


    printf("= averageWaitingTime: %lf\n", (averageWaitingTime/(double) nprocs) );
    printf("= averageResponseTime: %lf\n", (averageResponseTime/(double) nprocs) );
    printf("= averageReturnTimeN: %lf\n", (averageReturnTimeN/(double) nprocs) );
    printf("= averageReturnTime: %lf\n", (averageReturnTime/(double) nprocs) );

}
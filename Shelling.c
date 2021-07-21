#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define SIZE 20 //Indica il numero di righe e colonne che la matrice avrà
#define TOLLERANCE 51 //Indica la tolleranza che devono avere gli elementi della matrice
#define PERC_O 50 //Viene indicata la percentuale di elementi 'O' all'interno della matrice
#define PERC_X 100-PERC_O //Viene indicata la percentuale di elementi 'X' all'interno della matrice
#define WHITE_SPACES 30 //Viene indicata la percentuale di caselle vuote all'interno della matrice
#define ITERATIONS 1000000 //Viene indicato il numero massimo di iterazioni che l'algoritmo esegue
#define RANDOM_MATRIX 1 //Con 0 viene generata una matrice costante, con 1 una matrice casuale

typedef struct 
{
    int x;
    int y;
}Coord; //La struttura Coord viene utilizzata per salvare la posizione dei vari elementi

typedef struct {
    int array_border[2][SIZE]; //array per salvare le righe dei processi vicini
     int **array_op; //array per andare a salvare le righe che verranno utilizzate durante la computazione da ciascun processo
     Coord *array_empty_coord; //array per salvare le posizioni di tutti gli elementi vuoti presenti all'interno della matrice
     Coord *array_local_empty_coord; //array per salvare le posizioni degli elementi vuoti appartenenente ad un processo
     int rows_op; //utilizzato per tenere traccia del quantitativo di righe che un processo possiede
     int initial_row; //utilizzato per tenere traccia la posizione della riga iniziale della porzione di matrice appartenente ad un processo
     int dim_array_local_empty; //utilizzato per tenere traccia del numero di celle vuote appartenenti ad un processo.
     } MatElements;

typedef struct 
{
    int rank;
    int val;
    int x;
    int y;
    int prev_x;
    int prev_y;
    int changed;
} Element; //Questa struttura viene utilizzata per tenere traccia degli elementi scambiati dai diversi processori



MatElements sarray;
Element *elements;


int size_elements=0;

int stop=1;

int numb_white_spaces=SIZE*SIZE*WHITE_SPACES/100;


int array_completo[SIZE][SIZE];


int check(int array_border[],int size); //Controlla se l'elemento rispetta i parametri di tolleranza all'interno della matrice
void swap(int i,int j,int world_rank,int rows); //Effettua lo swap dell'elemento all'interno della matrice
void print_matrix_complete(); //Viene stampata la matrice completa
void print_matrix_op(); //Viene stampata la matrice appartenente al singolo processo
void def_array_border(int world_rank,int world_size); //Permette lo scambio di righe di bordo tra i vari processi
void find_places(int world_rank, int world_size); //Utilizzata per iterare gli elementi all'interno della matrice, controllando se ogni elemento è soddisfatto

int main(int argc, char** argv) {
    MPI_Init(NULL, NULL);
    
    MPI_Status stat;

    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
   
    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    MPI_Request request[world_size];

    int send_counts[world_size];
    int displs[world_size];

    MPI_Datatype elementswap,oldtypes[1];
    int blockcounts[1]={7};
    MPI_Aint    offsets[1], lb, extent;
    offsets[0]=0;
    oldtypes[0] = MPI_INT;
    blockcounts[0]=7;
    MPI_Type_create_struct(1, blockcounts, offsets, oldtypes, &elementswap);
    MPI_Type_commit(&elementswap);

    MPI_Datatype coordswap;
    blockcounts[0]=2;
    MPI_Type_create_struct(1, blockcounts, offsets, oldtypes, &coordswap);
    MPI_Type_commit(&coordswap);
    
    int char_arr[]={88,79};
    elements=malloc(sizeof(Element));
    sarray.array_empty_coord=malloc(sizeof(Coord)*numb_white_spaces);

    int all_int_info[world_size*2+numb_white_spaces*2];
    

    int count_stop=0;
    
    
    int numb_o=(SIZE*SIZE-numb_white_spaces)*PERC_O/100;
    int numb_x=SIZE*SIZE-numb_white_spaces-numb_o;

    int count_o=0;
    int count_x=0;


    int initial_row[world_size];

    if(SIZE<world_size) //Viene fatto un controllo se il numero dei processori sia almeno uguale al numero di righe della matrice
    {
        if(world_rank == 0)
            printf("Dare almeno 1 processore per riga di matrice\n");
        MPI_Abort(MPI_COMM_WORLD,MPI_ERR_OP);
        
    }
    
    //Il rank 0 provvede a riempire in modo randomico la matrice con i parametri associati agli elementi
    if(world_rank == 0)
    {
        printf("numb white spaces:%d numb_o:%d, numb_x:%d\n",numb_white_spaces,numb_o,numb_x);
            int counter_for_empty=0;
            if(RANDOM_MATRIX == 0)
            {
                
                int char_array[]={79,32,88};
                for(int i=0;i<SIZE;i++)
                    for(int j=0;j<SIZE;j++)
                    {
                        srand(i*j);
                        int random_number=rand();
                        
                        if(char_array[random_number%3]==32 && counter_for_empty < numb_white_spaces)
                        {
                            array_completo[i][j]=char_array[random_number%3];
                            sarray.array_empty_coord[counter_for_empty].x=i;
                            sarray.array_empty_coord[counter_for_empty].y=j;
                            counter_for_empty++;
                        }
                        else if(char_array[random_number%3]!= 32)
                            array_completo[i][j]=char_array[random_number%3];
                        else
                            array_completo[i][j]=char_arr[random_number%2];
                    }
            }           
            else
            {
                int seed_one=0;
                int seed_two=0;
                while (count_o < numb_o || count_x < numb_x)
                {
                    seed_one++;
                    seed_two+=3;
                    srand(time(NULL)+seed_two);
                    int x_two = rand()%SIZE;
                    srand(time(NULL)+seed_two*seed_one);
                    int y_two=rand()%SIZE;
                    srand(time(NULL)+seed_one*2);
                    int x=rand()%SIZE;
                    srand(time(NULL)+seed_one);
                    int y=rand()%SIZE;
                    if(array_completo[x][y]!=32 && array_completo[x][y]!= 88 && array_completo[x][y] != 79 && count_o < numb_o)
                    {   
                        array_completo[x][y]=79;
                        count_o++;     
                    }
                    if(array_completo[x_two][y_two]!=32 && array_completo[x_two][y_two]!= 88 && array_completo[x_two][y_two] != 79 && count_x < numb_x)
                    {
                        array_completo[x_two][y_two]=88;
                        count_x++;
                    }
                }
                
                count_x=0;
                count_o=0;
                for(int i=0;i<SIZE;i++)
                {
                     for(int j=0;j<SIZE;j++)
                    {
                        
                        if(array_completo[i][j]!=88 && array_completo[i][j]!= 79)
                        {
                            array_completo[i][j]=32;
                            sarray.array_empty_coord[counter_for_empty].x=i;
                            sarray.array_empty_coord[counter_for_empty].y=j;
                            counter_for_empty++;
                        }
                        else if(array_completo[i][j]==79)
                            count_o++;
                        else if(array_completo[i][j]==88)
                            count_x++;
                    }         
                }  
                while(counter_for_empty < numb_white_spaces)
                {
                    int x=rand()%SIZE;
                    srand(time(NULL));
                    int y=rand()%SIZE;
                    if(count_o>numb_o || count_x > numb_x)
                    {
                        if(array_completo[x][y]==79 && count_o>numb_o)
                        {
                            array_completo[x][y]=32;
                            sarray.array_empty_coord[counter_for_empty].x=x;
                            sarray.array_empty_coord[counter_for_empty].y=y;
                            counter_for_empty++;
                            count_o--;
                        }
                        else if(array_completo[x][y]==88 && count_x>numb_x)
                        {
                            array_completo[x][y]=32;
                            sarray.array_empty_coord[counter_for_empty].x=x;
                            sarray.array_empty_coord[counter_for_empty].y=y;
                            counter_for_empty++;
                            count_x--;
                        }
                    }
                }
            }
            
       
        //print_matrix_complete();


        int modulo=SIZE%world_size;
        int div=SIZE/world_size;
        int check=0;
        for(int i=0;i<world_size;i++)
        {
            if(modulo>0)
            {
                send_counts[i]=SIZE*(div+1);
                if(i==0)
                {
                    initial_row[0]=0;
                    displs[i]=0;
                } 
                else
                {
                    
                    displs[i]=displs[i-1]+send_counts[i-1];
                    initial_row[i]=displs[i]/SIZE;
                }
                modulo--;
                check=1;
            
            }
            else
            {
                send_counts[i]=SIZE*div;
                if(i==0)
                {
                    initial_row[0]=0;
                    displs[i]=0;
                }
                else
                {
                    if(check==1)
                    {
                        displs[i]=displs[i-1]+send_counts[i-1];
                        check=0;
                    }
                    else
                    {
                        displs[i]=displs[i-1]+send_counts[i-1];
                    }
                    initial_row[i]=displs[i]/SIZE;
                    
                }
            }
        }
        //All'interno dell'array all_int_info vengono salvate tutte le informazioni riguardanti gli elementi che possiede ciascun processore, la riga iniziale e le coordinate degli elementi vuoti
         for(int i=0;i<world_size*2;i++)
            all_int_info[i]= (i<world_size) ? send_counts[i] : initial_row[i-world_size];

         for(int i=0,j=0;j<numb_white_spaces;i+=2,j++)
         {
             all_int_info[world_size*2+i]=sarray.array_empty_coord[j].x;
             all_int_info[world_size*2+i+1]=sarray.array_empty_coord[j].y;             
         }
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double start, end;
    start=MPI_Wtime();
    
    if(world_size>1)
        MPI_Bcast(all_int_info, world_size*2 + numb_white_spaces*2, MPI_INT, 0,MPI_COMM_WORLD);

    sarray.dim_array_local_empty=0;

    if(world_rank != 0)
    {   
         int check_coord=0;
         for(int i=0;i<world_size*2;i++)
        {
            if(i<world_size)
                send_counts[i]=all_int_info[i];
            else if(i >= world_size && i < world_size*2)
                initial_row[i-world_size]=all_int_info[i];
        }
        for(int i=0,j=0;j<numb_white_spaces;i+=2,j++)
         {
             sarray.array_empty_coord[j].x=all_int_info[world_size*2+i];
             sarray.array_empty_coord[j].y=all_int_info[world_size*2+i+1];
         }
    }   
   
    
    
    int rows=send_counts[world_rank]/SIZE;
    sarray.rows_op=rows;
    sarray.initial_row=initial_row[world_rank];

    for(int i=0;i<numb_white_spaces;i++)
    {
        int empty_slot_x=sarray.array_empty_coord[i].x;
        if( empty_slot_x < sarray.initial_row+rows && empty_slot_x >= sarray.initial_row)
        {
            sarray.dim_array_local_empty++;
            sarray.array_local_empty_coord=realloc(sarray.array_local_empty_coord,sizeof(Coord)*sarray.dim_array_local_empty);
            sarray.array_local_empty_coord[sarray.dim_array_local_empty-1].x=sarray.array_empty_coord[i].x;
            sarray.array_local_empty_coord[sarray.dim_array_local_empty-1].y=sarray.array_empty_coord[i].y;
        }
    }

    
    int col=SIZE;
    int *ptr;
    
    int len=sizeof(int *) * rows + sizeof(int) * col * rows;
    int count=0;
    

    sarray.array_op = (int **)malloc(len);

    ptr = (int *)(sarray.array_op + rows);
    for(int i = 0; i < rows; i++)
        sarray.array_op[i] = (ptr + col * i);


    MPI_Scatterv(array_completo, send_counts, displs, MPI_INT,&sarray.array_op[0][0], send_counts[world_rank], MPI_INT, 0, MPI_COMM_WORLD);
    
    
    def_array_border(world_rank,world_size);

   int count_iterations=0;
   count_stop=0;
   int stop_iterations[world_size];
   int round_check=0;
   
    for(int i=0;i<ITERATIONS;i++){
        if(world_size > 1)
            MPI_Allgather(&stop,1,MPI_INT,stop_iterations,1,MPI_INT,MPI_COMM_WORLD);
        else
            stop_iterations[0]=stop;

        for(int k=0;k<world_size;k++)
            if(stop_iterations[k]==0)
                count_stop++;
                
        if(count_stop==world_size)
            break;


    count_stop=0;
    count_iterations++;
    round_check++;
    stop=0;

    find_places(world_rank,world_size);

    if(world_size > 1)
    {
        int size_all_elements_array[world_size];

        MPI_Allgather(&size_elements, 1, MPI_INT, size_all_elements_array, 1, MPI_INT,MPI_COMM_WORLD);
        
        int size_all_elements=0;
        int counts_swap[world_size];
        int displacements_swap[world_size];
        for(int i=0;i<world_size;i++)
        {
            size_all_elements+=size_all_elements_array[i];
            counts_swap[i]=size_all_elements_array[i];
            displacements_swap[i]= (i==0)? 0 : displacements_swap[i-1]+size_all_elements_array[i-1];
        }


        Element all_elements[size_all_elements];

        MPI_Allgatherv(elements, size_elements, elementswap, all_elements, counts_swap, displacements_swap, elementswap, MPI_COMM_WORLD);
        

        Element *elements_changed=malloc(sizeof(Element));

        
        int size_elements_changed=0;
        for(int i=0;i<size_all_elements;i++)
        {
            int coord_x=all_elements[i].x;
            int coord_y=all_elements[i].y;
            
            for(int j=0;j<sarray.dim_array_local_empty;j++)
            {
                if(coord_x== sarray.array_local_empty_coord[j].x && coord_y == sarray.array_local_empty_coord[j].y)
                {
                    all_elements[i].changed=1;
                    sarray.array_local_empty_coord[j]=sarray.array_local_empty_coord[sarray.dim_array_local_empty-1];
                    sarray.dim_array_local_empty--;
                    sarray.array_local_empty_coord=realloc(sarray.array_local_empty_coord,sizeof(Coord)*sarray.dim_array_local_empty);
                    int x=all_elements[i].x;
                    sarray.array_op[x-sarray.initial_row][all_elements[i].y]=all_elements[i].val;
                    size_elements_changed++;
                    elements_changed=realloc(elements_changed,sizeof(Element)*size_elements_changed);
                    elements_changed[size_elements_changed-1]=all_elements[i];
                }
            }
        }

        int size_all_elements_changed_array[world_size];

        
        MPI_Allgather(&size_elements_changed, 1, MPI_INT, size_all_elements_changed_array, 1, MPI_INT,MPI_COMM_WORLD);

        int size_all_elements_changed=0;
        int counts_swap_changed[world_size];
        int displacements_swap_changed[world_size];

        for(int i=0;i<world_size;i++)
        {
            size_all_elements_changed+=size_all_elements_changed_array[i];
            counts_swap_changed[i]=size_all_elements_changed_array[i];
            displacements_swap_changed[i] = (i==0) ? 0 : displacements_swap_changed[i-1]+size_all_elements_changed_array[i-1];
        }

        Element all_elements_changed[size_all_elements_changed];

        if(size_all_elements_changed>0)
            MPI_Allgatherv(elements_changed, size_elements_changed, elementswap, all_elements_changed, counts_swap_changed, displacements_swap_changed, elementswap, MPI_COMM_WORLD);


        for(int i=0; i< size_all_elements_changed;i++)
        {
            if(all_elements_changed[i].rank==world_rank)
            {
                sarray.array_op[all_elements_changed[i].prev_x-sarray.initial_row][all_elements_changed[i].prev_y]=32;
                sarray.dim_array_local_empty++;
                sarray.array_local_empty_coord=realloc(sarray.array_local_empty_coord,sizeof(Coord)*sarray.dim_array_local_empty);
                Coord coord;
                coord.x=all_elements_changed[i].prev_x;
                coord.y=all_elements_changed[i].prev_y;
                sarray.array_local_empty_coord[sarray.dim_array_local_empty-1]=coord;
                
            }
        }
     
        int size_all_elements_local_empty[world_size];
        
        MPI_Allgather(&sarray.dim_array_local_empty, 1, MPI_INT, size_all_elements_local_empty, 1, MPI_INT,MPI_COMM_WORLD);

        
        int counts_swap_coord[world_size];
        int displacements_swap_coord[world_size];

    
        for(int i=0;i<world_size;i++)
        {
            counts_swap_coord[i]=size_all_elements_local_empty[i];
            displacements_swap_coord[i] = (i==0)? 0: displacements_swap_coord[i-1]+size_all_elements_local_empty[i-1];               
        }

        MPI_Allgatherv(sarray.array_local_empty_coord,sarray.dim_array_local_empty,coordswap,sarray.array_empty_coord,counts_swap_coord,displacements_swap_coord,coordswap,MPI_COMM_WORLD);

        def_array_border(world_rank,world_size);

        
        free(elements_changed);
        elements=realloc(elements,0);
        size_elements=0;
        size_elements_changed=0;
        }
    }

    if(world_size > 1)
    {
        int send_rows[world_size];

        MPI_Allgather(&send_counts[world_rank],1,MPI_INT,send_rows,1,MPI_INT,MPI_COMM_WORLD);

        int displs_rows[world_size];
        for(int i=0;i<world_size;i++)
            displs_rows[i]= (i==0) ? 0 : displs_rows[i-1]+send_rows[i-1];

        MPI_Gatherv(&sarray.array_op[0][0],send_rows[world_rank],MPI_INT,&array_completo[0][0],send_rows,displs_rows,MPI_INT,0,MPI_COMM_WORLD);
    }
    
    
    
    
    
    end = MPI_Wtime();
    
    if(world_rank == 0)
    {
        if(count_iterations == ITERATIONS)
        printf("sono il processo:%d e non sono riuscito a risolvere la matrice, tempo impiegato:%f\n",world_rank,end-start);
            
        else
            printf("sono il processo:%d ho risolto la matrice ed il tempo impiegato è stato di:%f con un totale di %d iterazioni\n",world_rank,end-start,count_iterations);
        print_matrix_complete();
    }
   
    MPI_Type_free(&elementswap);
    MPI_Type_free(&coordswap);
    MPI_Finalize();
}



int check(int array_border[],int size)
{   
    int count=0;
    int count_to_remove=0;
    
    if(array_border[0]==32)
    {
        return 0;
    }
    for(int i = 1 ; i< size;i++)
    {
        if(array_border[0]==array_border[i])
            count++;
        if(array_border[i]==32)
            count_to_remove++;
        
    }
    size--;
    int size_final=size-count_to_remove;
    int final_result;
    if(size_final==0)
        final_result=100;
    else
        final_result=(count*100)/size_final;

    if(final_result < TOLLERANCE )
    {
       return 1;
    }
    else
    {
        return 0;
    } 
}


void swap(int i,int j,int world_rank,int rows)
{
    stop=1;
    srand(time(NULL) + world_rank+i+j + world_rank*i*j);
    int r=rand()%numb_white_spaces;
    int check=0;
   
    int coord_empty_x=sarray.array_empty_coord[r].x;
    int coord_empty_y=sarray.array_empty_coord[r].y;

    int local_i;

    for(int i=0;i<sarray.dim_array_local_empty;i++)
    {
        if(coord_empty_x==sarray.array_local_empty_coord[i].x && coord_empty_y == sarray.array_local_empty_coord[i].y)
        {
            check=1;
            local_i=i;
        }
            
    }
   
    if(check==1)
    {
        {
            sarray.array_empty_coord[r].x=i+sarray.initial_row;
            sarray.array_empty_coord[r].y=j;
            sarray.array_local_empty_coord[local_i].x=i+sarray.initial_row;
            sarray.array_local_empty_coord[local_i].y=j;
            sarray.array_op[coord_empty_x-sarray.initial_row][coord_empty_y]=sarray.array_op[i][j];
            sarray.array_op[i][j]=32;
        }     
    }
    else
    {
        int check_elements=0;
        for(int i=0;i< size_elements;i++)
        {
            if(elements[i].x==coord_empty_x && elements[i].y == coord_empty_y)
                check_elements=1;
        }
        if(check_elements==0)
        {
            Element elem;
            elem.changed=0;
            elem.rank=world_rank;
            elem.val=sarray.array_op[i][j];
            elem.prev_x=i+sarray.initial_row;
            elem.prev_y=j;
            elem.x=coord_empty_x;
            elem.y=coord_empty_y;
            size_elements++;
            elements=realloc(elements,sizeof(Element)*size_elements);
            elements[size_elements-1]=elem;
        }
    }
}

void print_matrix_complete()
{
      for(int i=0;i<SIZE;i++)
        {
            for(int j=0;j<SIZE;j++)
            {
                if(array_completo[i][j]=='X')
                    printf("\033[1;33m%c ",array_completo[i][j]);
                else
                    printf("\033[1;31m%c ",array_completo[i][j]);
            }
            printf("\n");
        }
        printf("\033[0m");
}

// void print_matrix_op()
// {
//     for(int i =0;i< sarray.rows_op;i++)
//     {
//         for(int j=0;j< SIZE;j++)
//         {
//             printf("%c ",sarray.array_op[i][j]);
//         }
//         printf("\n");
//     }
// }

// void print_empty()
// {
//     for(int i=0;i<numb_white_spaces;i++)
//     {
//         printf("%d%d ",sarray.array_empty_coord[i].x,sarray.array_empty_coord[i].y);
//     }

// }

void print_local_empty()
{
    for(int i=0;i<sarray.dim_array_local_empty;i++)
    {
        printf("%d%d ",sarray.array_local_empty_coord[i].x,sarray.array_local_empty_coord[i].y);
    }
}

void def_array_border(int world_rank,int world_size)
{
    if(world_size > 1)
    {
        if(world_rank == 0)
        {
            MPI_Status status;
            MPI_Sendrecv(&sarray.array_op[sarray.rows_op-1][0], SIZE, MPI_INT, world_rank+1, 0,&sarray.array_border[0][0], SIZE, MPI_INT, world_rank+1,0, MPI_COMM_WORLD, &status);
        }
        else if(world_rank == world_size -1)
        { 
            MPI_Status status;
            MPI_Sendrecv(&sarray.array_op[0][0], SIZE, MPI_INT, world_rank-1, 0,&sarray.array_border[0][0], SIZE, MPI_INT, world_rank-1,0, MPI_COMM_WORLD, &status);
        }
        else
        {
            MPI_Status status;
            MPI_Sendrecv(&sarray.array_op[0][0], SIZE, MPI_INT, world_rank-1, 0,&sarray.array_border[0][0], SIZE, MPI_INT, world_rank-1,0, MPI_COMM_WORLD, &status);
            MPI_Sendrecv(&sarray.array_op[sarray.rows_op-1][0], SIZE, MPI_INT, world_rank+1, 0,&sarray.array_border[1][0], SIZE, MPI_INT, world_rank+1,0, MPI_COMM_WORLD, &status);
        }
    }    
}

void find_places(int world_rank, int world_size)
{
    for(int i=0;i<sarray.rows_op;i++)
            {
                for(int j=0;j<SIZE;j++)
                {
                    int *array_border_pointer;
                    int size_array_border;
                    if(j==0 && i == 0)
                    {
                            if(world_rank == 0)
                            {
                                if(sarray.rows_op==1)
                                {
                                    int array_border[]={sarray.array_op[i][j],sarray.array_border[0][j],sarray.array_border[0][j+1],sarray.array_op[i][j+1]};
                                    size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                                    array_border_pointer=array_border;

                                }
                                else
                                {
                                    int array_border[]={sarray.array_op[i][j],sarray.array_op[i+1][j],sarray.array_op[i+1][j+1],sarray.array_op[i][j+1]};
                                    size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                                    array_border_pointer=array_border;
                                }
                            }
                            else if(world_rank == world_size-1)
                            {
                                if(sarray.rows_op==1)
                                {
                                    int array_border[]={sarray.array_op[i][j],sarray.array_border[0][j],sarray.array_border[0][j+1],sarray.array_op[i][j+1]};
                                    size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                                    array_border_pointer=array_border;

                                }
                                else
                                {
                                    int array_border[]={sarray.array_op[i][j],sarray.array_border[0][j],sarray.array_border[0][j+1],sarray.array_op[i][j+1],sarray.array_op[i+1][j],sarray.array_op[i+1][j+1]};
                                    size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                                    array_border_pointer=array_border;
                                }
                            }
                            else
                            {
                                if(sarray.rows_op==1)
                                {
                                    int array_border[]={sarray.array_op[i][j],sarray.array_border[0][j],sarray.array_border[0][j+1],sarray.array_op[i][j+1],sarray.array_border[1][j],sarray.array_border[1][j+1]};
                                    size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                                    array_border_pointer=array_border;

                                }
                                else
                                {
                                    int array_border[]={sarray.array_op[i][j],sarray.array_border[0][j],sarray.array_border[0][j+1],sarray.array_op[i][j+1],sarray.array_op[i+1][j],sarray.array_op[i+1][j+1]};
                                    size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                                    array_border_pointer=array_border;
                                }
                            }
                    }
                    else if(i==0 && j==SIZE-1)
                    {
                        if(world_rank == 0)
                        {
                            
                            if(sarray.rows_op==1)
                            {
                                int array_border[]={sarray.array_op[i][j],sarray.array_border[0][j],sarray.array_border[0][j-1],sarray.array_op[i][j-1]};
                                size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                                array_border_pointer=array_border;

                            }
                            else
                            {
                                int array_border[]={sarray.array_op[i][j],sarray.array_op[i+1][j],sarray.array_op[i+1][j-1],sarray.array_op[i][j-1]};
                                size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                                array_border_pointer=array_border;
                            }
                        }
                        else if(world_rank == world_size-1)
                        {
                            if(sarray.rows_op==1)
                            {
                                int array_border[]={sarray.array_op[i][j],sarray.array_border[0][j],sarray.array_border[0][j-1],sarray.array_op[i][j-1]};
                                size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                                array_border_pointer=array_border;

                            }
                            else
                            {
                                int array_border[]={sarray.array_op[i][j],sarray.array_border[0][j],sarray.array_border[0][j-1],sarray.array_op[i][j-1],sarray.array_op[i+1][j],sarray.array_op[i+1][j-1]};
                                size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                                array_border_pointer=array_border;
                            }
                        }
                        else
                        {
                            if(sarray.rows_op==1)
                            {
                                int array_border[]={sarray.array_op[i][j],sarray.array_border[0][j],sarray.array_border[0][j-1],sarray.array_op[i][j-1],sarray.array_border[1][j],sarray.array_border[1][j-1]};
                                size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                                array_border_pointer=array_border;

                            }
                            else
                            {
                                
                                int array_border[]={sarray.array_op[i][j],sarray.array_border[0][j],sarray.array_border[0][j-1],sarray.array_op[i][j-1],sarray.array_op[i+1][j],sarray.array_op[i+1][j-1]};
                                size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                                array_border_pointer=array_border;
                            }
                        }
                    }
                    else if(i==0 && (j>0 && j<SIZE-1) )
                    {
                        if(world_rank == 0)
                        {
                            
                            if(sarray.rows_op==1)
                            {
                                int array_border[]={sarray.array_op[i][j],sarray.array_border[0][j],sarray.array_border[0][j-1],sarray.array_border[0][j+1],sarray.array_op[i][j-1],sarray.array_op[i][j+1]};
                                size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                                array_border_pointer=array_border;

                            }
                            else
                            {
                                int array_border[]={sarray.array_op[i][j],sarray.array_op[i+1][j],sarray.array_op[i+1][j-1],sarray.array_op[i+1][j+1],sarray.array_op[i][j-1],sarray.array_op[i][j+1]};
                                size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                                array_border_pointer=array_border;
                            }
                        }
                        else if(world_rank == world_size-1)
                        {
                            if(sarray.rows_op==1)
                            {
                                int array_border[]={sarray.array_op[i][j],sarray.array_border[0][j],sarray.array_border[0][j-1],sarray.array_border[0][j+1],sarray.array_op[i][j-1],sarray.array_op[i][j+1]};
                                size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                                array_border_pointer=array_border;

                            }
                            else
                            {
                                
                                int array_border[]={sarray.array_op[i][j],sarray.array_border[0][j],sarray.array_border[0][j-1],sarray.array_border[0][j+1],sarray.array_op[i][j-1],sarray.array_op[i][j+1],sarray.array_op[i+1][j],sarray.array_op[i+1][j-1],sarray.array_op[i+1][j+1]};
                                size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                                array_border_pointer=array_border;
                            }
                        }
                        else
                        {
                            if(sarray.rows_op==1)
                            {
                                int array_border[]={sarray.array_op[i][j],sarray.array_border[0][j],sarray.array_border[0][j-1],sarray.array_border[0][j+1],sarray.array_op[i][j-1],sarray.array_op[i][j+1],sarray.array_border[1][j],sarray.array_border[1][j-1],sarray.array_border[1][j+1]};
                                size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                                array_border_pointer=array_border;

                            }
                            else
                            {
                                
                                int array_border[]={sarray.array_op[i][j],sarray.array_border[0][j],sarray.array_border[0][j-1],sarray.array_border[0][j+1],sarray.array_op[i][j-1],sarray.array_op[i][j+1],sarray.array_op[i+1][j],sarray.array_op[i+1][j-1],sarray.array_op[i+1][j+1]};
                                size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                                array_border_pointer=array_border;
                            
                            }
                        }
                    }
                    else if(j==0 && (i>0 && i<sarray.rows_op-1))
                    {
                        int array_border[]={sarray.array_op[i][j],sarray.array_op[i-1][j],sarray.array_op[i-1][j+1],sarray.array_op[i][j+1],sarray.array_op[i+1][j],sarray.array_op[i+1][j+1]};
                        size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                        array_border_pointer=array_border;    
                    }
                    else if(j==SIZE-1 && (i>0 && i<sarray.rows_op-1))
                    {
                        int array_border[]={sarray.array_op[i][j],sarray.array_op[i-1][j],sarray.array_op[i-1][j-1],sarray.array_op[i][j-1],sarray.array_op[i+1][j],sarray.array_op[i+1][j-1]};
                        size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                        array_border_pointer=array_border;
                    
                    }
                    else if((i>0 && i<sarray.rows_op-1) && (j>0 && j<SIZE -1))
                    {
                    
                        int array_border[]={sarray.array_op[i][j],sarray.array_op[i-1][j],sarray.array_op[i-1][j-1],sarray.array_op[i-1][j+1],sarray.array_op[i][j-1],sarray.array_op[i][j+1],sarray.array_op[i+1][j-1],sarray.array_op[i+1][j],sarray.array_op[i+1][j+1]};
                        size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                        array_border_pointer=array_border;
                    
                    }
                    else if(j == 0 && i==sarray.rows_op-1)
                    {
                        if(world_rank == 0)
                        {
                            int array_border[]={sarray.array_op[i][j],sarray.array_op[i-1][j],sarray.array_op[i-1][j+1],sarray.array_op[i][j+1],sarray.array_border[0][j],sarray.array_border[0][j+1]};
                            size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                            array_border_pointer=array_border;
                        }
                        else if(world_rank == world_size-1)
                        {
                        
                            int array_border[]={sarray.array_op[i][j],sarray.array_op[i-1][j],sarray.array_op[i-1][j+1],sarray.array_op[i][j+1]};
                            size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                            array_border_pointer=array_border;
                        }
                        else
                        {  
                            int array_border[]={sarray.array_op[i][j],sarray.array_border[1][j],sarray.array_border[1][j+1],sarray.array_op[i][j+1],sarray.array_op[i-1][j],sarray.array_op[i-1][j+1]};
                            size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                            array_border_pointer=array_border;
                        }
                        
                    }
                    else if(j==SIZE-1 && i==sarray.rows_op-1)
                    {
                        if(world_rank == 0)
                        {
                            int array_border[]={sarray.array_op[i][j],sarray.array_op[i-1][j],sarray.array_op[i-1][j-1],sarray.array_op[i][j-1],sarray.array_border[0][j],sarray.array_border[0][j-1]};
                            size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                            array_border_pointer=array_border;
                    
                        }
                        else if(world_rank == world_size-1)
                        {
                            
                            int array_border[]={sarray.array_op[i][j],sarray.array_op[i-1][j],sarray.array_op[i-1][j-1],sarray.array_op[i][j-1]};
                            size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                            array_border_pointer=array_border;
                            
                        }
                        else
                        {
                            
                            int array_border[]={sarray.array_op[i][j],sarray.array_border[1][j],sarray.array_border[1][j-1],sarray.array_op[i][j-1],sarray.array_op[i-1][j],sarray.array_op[i-1][j-1]};
                            size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                            array_border_pointer=array_border;
                        } 
                    }
                    else
                    {
                        
                        if(world_rank == 0)
                        {
                            int array_border[]={sarray.array_op[i][j],sarray.array_op[i-1][j],sarray.array_op[i-1][j-1],sarray.array_op[i-1][j+1],sarray.array_op[i][j-1],sarray.array_op[i][j+1],sarray.array_border[0][j-1],sarray.array_border[0][j],sarray.array_border[0][j+1]};
                            size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                                array_border_pointer=array_border;
                        }
                        else if(world_rank == world_size - 1)   
                        {
                            int array_border[]={sarray.array_op[i][j],sarray.array_op[i][j-1],sarray.array_op[i][j+1],sarray.array_op[i-1][j-1],sarray.array_op[i-1][j],sarray.array_op[i-1][j+1]};
                            size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                                array_border_pointer=array_border;
                        }
                        else
                        {
                            int array_border[]={sarray.array_op[i][j],sarray.array_op[i-1][j],sarray.array_op[i-1][j-1],sarray.array_op[i-1][j+1], sarray.array_op[i][j-1], sarray.array_op[i][j+1], sarray.array_border[1][j],sarray.array_border[1][j-1],sarray.array_border[1][j+1]};
                            size_array_border=sizeof(array_border)/sizeof(array_border[0]);
                            array_border_pointer=array_border;
                            
                        }

                    }
                    if(check(array_border_pointer,size_array_border))
                    {
                        
                        swap(i,j,world_rank,sarray.rows_op);
                    }
                }
            
            }
} 
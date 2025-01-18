#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>



//  NAME:       Utku
//  SURNAME:    Genc
//  SUID:       utku.genc
//  SUNUMBER:   30611
//  COURSE:     CS307

int main (int argc, char *argv[])
{
    if (argc != 4) {
        printf("Usage: treePipe <current depth> <max depth> <left-right>\n");
        return 1;
    }
    int error_num1, error_num2, flag;
    char extra;

    //Validate the first argument
    if (sscanf(argv[1], "%d%c", &error_num1, &extra) != 1 || error_num1 < 0)
    {
        printf("Usage: treePipe <current depth> <max depth> <left-right>\n");
        return 1;
    }

    //Validate the second argument
    if (sscanf(argv[2], "%d%c", &error_num2, &extra) != 1 || error_num2 < 0)
    {
        printf("Usage: treePipe <current depth> <max depth> <left-right>\n");
        return 1;
    }
    //Argument 1 should be smaller than argument 2
    if (error_num2 < error_num1)
    {
        printf("Usage: treePipe <current depth> <max depth> <left-right>\n");
        return 1;
    }

    //Validate the third argument
    if (sscanf(argv[3], "%d%c", &flag, &extra) != 1 || (flag != 0 && flag != 1))
    {
        printf("Usage: treePipe <current depth> <max depth> <left-right>\n");
        return 1;
    }


    int num1;
    int num2 = 1;
    

    int default_value = 1;
    int current_depth = atoi(argv[1]);
    int max_depth = atoi(argv[2]);
    int calculation_program = atoi(argv[3]);

    for (int i = 0; i < current_depth; i++)
    {
        fprintf(stderr, "---");
    }
    fprintf(stderr, "> Current depth: %d, lr: %d\n", current_depth, calculation_program);


    if (current_depth == 0)
    {
        printf("Please enter num1 for the root: ");
        scanf("%d", &num1);
    }
    else
    {
        // Input will be scan from the parent.
        scanf("%d", &num1);
    }
    
    
    if (current_depth < max_depth)
    {
        // Create pipes for L child.
        int fd_parent_to_L_child[2];
        int fd_L_child_to_parent[2];
        pipe(fd_parent_to_L_child);
        pipe(fd_L_child_to_parent);

        pid_t id0 = fork();
        if (id0 == 0) 
        {
            //We are in the left child
            //Close the unused pipe ends
            close(fd_parent_to_L_child[1]);
            close(fd_L_child_to_parent[0]);

            //Redirect the pipes
            dup2(fd_parent_to_L_child[0], STDIN_FILENO);
            dup2(fd_L_child_to_parent[1], STDOUT_FILENO);

            //Close after dup2
            close(fd_parent_to_L_child[0]);
            close(fd_L_child_to_parent[1]); 

            //Update the depth and prepare the arguments for execvp
            char new_depth_L[12];
            sprintf(new_depth_L, "%d", current_depth + 1);
            char *args_L[] = {"./treePipe", new_depth_L, argv[2], "0", NULL};
            execvp(args_L[0], args_L);
        }
        else
        {
            //We are in parent
            //We should close the unused pipes
            close(fd_parent_to_L_child[0]);
            close(fd_L_child_to_parent[1]);

            //Parent should provide the num1 for its left children
            dprintf(fd_parent_to_L_child[1], "%d\n", num1);
            close(fd_parent_to_L_child[1]);

            //Waits for left children
            wait(NULL);

            //Read num1 from the left children
            char buffer[100];
            read(fd_L_child_to_parent[0], buffer, sizeof(buffer));
            close(fd_L_child_to_parent[0]);
            sscanf(buffer, "%d", &num1);

            //Parent should print the num1 after left children returns
            for (int i = 0; i < current_depth; i++)
            {
                fprintf(stderr, "---");
            }
            fprintf(stderr, "> My num1 is: %d\n", num1);
           
            //We should continue with the right child.
            //Create pipes for right children
            int fd_parent_to_R_child[2];
            int fd_R_child_to_parent[2];
            pipe(fd_parent_to_R_child);
            pipe(fd_R_child_to_parent);

            pid_t id1 = fork();
            if (id1 == 0) 
            {
                //We are in the right child
                //Close the unused pipe ends
                close(fd_parent_to_R_child[1]);
                close(fd_R_child_to_parent[0]);

                //Redirect the pipes
                dup2(fd_parent_to_R_child[0], STDIN_FILENO);
                dup2(fd_R_child_to_parent[1], STDOUT_FILENO);

                //Close after dup2
                close(fd_parent_to_R_child[0]);
                close(fd_R_child_to_parent[1]);


                //Update the depth and prepare the arguments for execvp
                char new_depth_R[12];
                sprintf(new_depth_R, "%d", current_depth + 1);
                char *args_R[] = {"./treePipe", new_depth_R, argv[2], "1", NULL};
                execvp(args_R[0], args_R);
            }
            else
            {
                //We are in parent
                //Close unused pipes
                close(fd_parent_to_R_child[0]);
                close(fd_R_child_to_parent[1]);

                //Parent should provide the num1 for right children
                dprintf(fd_parent_to_R_child[1], "%d\n", num1);
                close(fd_parent_to_R_child[1]);

                //Parent waits for right children for num2
                wait(NULL);

                //Parent reads from pipe to get num2
                char buffer_R[100];
                read(fd_R_child_to_parent[0], buffer_R, sizeof(buffer_R));
                close(fd_R_child_to_parent[0]);
                sscanf(buffer_R, "%d", &num2);
                

                //Now parent have num1 and num2
                //Creating pipes for worker
                int fd_parent_to_worker[2];
                int fd_worker_to_parent[2];
                pipe(fd_parent_to_worker);
                pipe(fd_worker_to_parent);

                pid_t id2 = fork();
                if (id2 == 0)
                {
                    //We are in worker children
                    //Close the unused pipes
                    close(fd_parent_to_worker[1]);
                    close(fd_worker_to_parent[0]);

                    //Redirect the file director for worker
                    dup2(fd_parent_to_worker[0], STDIN_FILENO);
                    dup2(fd_worker_to_parent[1], STDOUT_FILENO);

                    //Close after dup2
                    close(fd_parent_to_worker[0]);
                    close(fd_worker_to_parent[1]);

                    
                    //If lr is 0, this means parent is a left program
                    if (calculation_program == 0)
                    {
                        //There is no arguments for calculation program
                        char *args_calculation[] = {"./left", NULL};
                        execvp(args_calculation[0], args_calculation);
                    }
                    //Else lr == 1, which means parent is a right program
                    else
                    {
                        //There is no arguments for calculation program
                        char *args_calculation[] = {"./right", NULL};
                        execvp(args_calculation[0], args_calculation);
                    }
                }
                else
                {
                    //We are in parent
                    //Close unused pipes
                    close(fd_parent_to_worker[0]);
                    close(fd_worker_to_parent[1]);

                    //Parent should provide the left children num1 and num2
                    dprintf(fd_parent_to_worker[1], "%d\n", num1);
                    dprintf(fd_parent_to_worker[1], "%d\n", num2);
                    close(fd_parent_to_worker[1]);

                    //Parent should print out the current depth, lr, num1 and num2...
                    //...Before the calculation
                    for (int i = 0; i < current_depth; i++)
                    {
                        fprintf(stderr, "---");
                    }
                    fprintf(stderr, "> Current depth: %d, lr: %d, my num1: %d, my num2: %d\n"
                    ,current_depth, calculation_program, num1, num2);

                    //Wait calculation to be over
                    wait(NULL);
                    
                    //Read result from the worker pipe
                    char buffer[100];
                    read(fd_worker_to_parent[0], buffer, sizeof(buffer));
                    close(fd_worker_to_parent[0]);
                    sscanf(buffer, "%d", &num1);

                    //After the calculation parent should print out the result
                    //Parent should print out the current depth, lr, num1 and num2
                    for (int i = 0; i < current_depth; i++)
                    {
                        fprintf(stderr, "---");
                    }
                    fprintf(stderr, "> My result is: %d\n", num1);


                    //Print the result to parent- or terminal
                    //If we are in the root node
                    if (current_depth == 0)
                    {
                        printf("The final result is: %d\n", num1);
                    }
                    else
                    {
                        printf("%d\n", num1);
                    }
                   
                }
            }
        }
    }
    else
    {
        //This is a leaf node
        //This node should not generate children
        //Instead it should generate worker for calculation

        //Generate pipes for worker
        int fd_parent_to_worker[2];
        int fd_worker_to_parent[2];
        pipe(fd_parent_to_worker);
        pipe(fd_worker_to_parent);


        
        pid_t id2 = fork();
        if (id2 == 0)
        {
            //We are in worker children for leaf node

            //Close the unused pipe ends
            close(fd_parent_to_worker[1]);
            close(fd_worker_to_parent[0]);

            //Redirect the file directors
            dup2(fd_parent_to_worker[0], STDIN_FILENO);
            dup2(fd_worker_to_parent[1], STDOUT_FILENO);

            //Close after dup2
            close(fd_parent_to_worker[0]);
            close(fd_worker_to_parent[1]);

            //If this leaf node is a left program
            if (calculation_program == 0)
            {
                //Calculation program does not need any arguments
                char *args_calculation[] = {"./left", NULL};
                execvp(args_calculation[0], args_calculation);
            }
            //Else lr == 1, this leaf node is a right program
            else
            {
                //Calculation program does not need any arguments
                char *args_calculation[] = {"./right", NULL};
                execvp(args_calculation[0], args_calculation);
            }
        }
        else
        {
            //We are in leaf node
            //Close unused pipes
            close(fd_parent_to_worker[0]);
            close(fd_worker_to_parent[1]);

            //Leaf node should provide the num1 and default number to the worker
            dprintf(fd_parent_to_worker[1], "%d\n", num1);
            dprintf(fd_parent_to_worker[1], "%d\n", default_value);
            close(fd_parent_to_worker[1]);

            //Parent should print the num1 before the calculation
            for (int i = 0; i < current_depth; i++)
            {
                fprintf(stderr, "---");
            }
            fprintf(stderr, "> My num1 is: %d\n", num1);
            

            //Wait for the calculation to be done
            wait(NULL);

            //Leaf node reads result from the pipe
            char buffer[100];
            read(fd_worker_to_parent[0], buffer, sizeof(buffer));
            close(fd_worker_to_parent[0]);
            sscanf(buffer, "%d", &num1);

            //After getting the result parent should print the result
            for (int i = 0; i < current_depth; i++)
            {
                fprintf(stderr, "---");
            }
            fprintf(stderr, "> My result is: %d\n", num1);

            //Leaf node should send it to the parent or console
            if (current_depth == 0)
            {
                printf("The final result is: %d\n", num1);
            }
            else
            {
                printf("%d\n", num1);
            }
        }
        
    }



    return 0;
}
#include <iostream>
#include <unistd.h>
#include <vector>
#include <pthread.h>
#include <stdio.h>
using namespace std;

static pthread_mutex_t semB;
static pthread_cond_t empty, empty2 = PTHREAD_COND_INITIALIZER;

static bool DATABASE[10] = {false};
static int currPos, startGroup, posCount = 0;
static int group1 = 0, group2 = 0;
static int tempGroup1, tempGroup2;

int sleepTime = 0;
struct requests
{
    int user;
    int userGroup;
    int postion;
    int arrivalTime;
    int requestTime;
};

// empty: mutex cond variable for group(s)
// empty2: mutex cond variable for position(s)

void *req_func(void *arg)
{

    /******************************************************* GROUP MUTEX *******************************************************/
    pthread_mutex_lock(&semB);

    requests r = *(requests *)arg;

    cout << "User " << r.user << " from Group " << r.userGroup << " arrives to the DBMS" << endl;
    if (r.userGroup == 1)
    {
        group1++;
    }
    else
    {
        group2++;
    }

    if (r.userGroup != startGroup)
    {
        cout << "User " << r.user << " is waiting due to its group" << endl;
        while (group1 > 0)
            pthread_cond_wait(&empty, &semB);
    }
    else if (startGroup == 2 && r.userGroup != startGroup)
    {
        cout << "User " << r.user << " is waiting due to its group" << endl;
        while (group2 > 0)
            pthread_cond_wait(&empty, &semB);
    }

    pthread_mutex_unlock(&semB);
//    sleep(r.requestTime);
    /******************************************************* DATABASE MUTEX *******************************************************/

    pthread_mutex_lock(&semB);
    currPos = r.postion;
    if (!DATABASE[currPos])
    {
        posCount++;
        DATABASE[currPos] = true;
        cout << "User " << r.user << " is accessing the position " << r.postion << " of the database for " << r.requestTime << " second(s)" << endl;
    }
    else
    {
        cout << "User " << r.user << " is waiting: position " << r.postion << " of the database is being used by user 1" << endl;
        while (posCount > 0)
        {
            pthread_cond_wait(&empty2, &semB);
        }
        cout << "Other User " << r.user << " is accessing the position " << r.postion << " of the database for " << r.requestTime << " second(s)" << endl;
    }
    pthread_mutex_unlock(&semB);

    /******************************************************* LAST MUTEX *******************************************************/
    sleep(r.requestTime);
    pthread_mutex_lock(&semB);

    cout << "User " << r.user << " finished its execution" << endl;
    if (r.userGroup == 1)
    {
        group1--;
    }
    else if (r.userGroup == 2)
    {
        group2--;
    }
    // cout << "Current Pos: " << currPos << endl;
    DATABASE[currPos] = false;

    if (startGroup == 1 && group1 == 0)
    {
        pthread_cond_signal(&empty);
    }
    else if (startGroup == 2 && group2 == 0)
    {
        pthread_cond_signal(&empty);
    }

    if (startGroup == 1 && (group1 == 0 && group2 == tempGroup2))
    {
        // cout << "Group 2: " << group2 << endl;
        cout << "\nHello All users from Group " << r.userGroup << " finished their execution \nThe users from Group 2 start their execution\n"
             << endl;
    }
    else if (startGroup == 1 && (group1 == 0 && group2 == 0))
    {
        // cout << "Group 2 count " << group2 << endl;
        cout << "\nAll users from Group " << r.userGroup << " finished their execution \n";
    }

    if (startGroup == 2 && (group2 == 0 && group1 == tempGroup1))
    {
        cout << "\nAll users from Group " << r.userGroup << " finished their execution \nThe users from Group 1 start their execution\n"
             << endl;
    }
    else if (startGroup == 2 && (group1 == 0 && group2 == 0))
    {
        // cout << "Group 2" << endl;
        cout << "\nAll users from Group " << r.userGroup << " finished their execution \n";
    }

    if (!DATABASE[currPos])
    {
        posCount--;
        // cout << "Free " << r.user << " DATABASE" << endl;
        if (posCount == 0)
            pthread_cond_signal(&empty2);
    }
    pthread_mutex_unlock(&semB);
    // pthread_exit((void *)0);
     return NULL;
}
int main()
{
    int user = 1, group, pos, aTime, rTime;
    vector<requests> reqs;
    cin >> startGroup;
    while (cin >> group >> pos >> aTime >> rTime)
    {
        requests r;
        r.user = user;
        r.userGroup = group;
        r.postion = pos;
        r.arrivalTime = aTime;
        r.requestTime = rTime;
        reqs.push_back(r);
        user++;
        if (group == 1)
        {
            tempGroup1++;
        }
        else
        {
            tempGroup2++;
        }
    }

    pthread_t tid[reqs.size()];
    pthread_mutex_init(&semB, NULL);
    for (int i = 0; i < reqs.size(); i++)
    {
        if (pthread_create(&tid[i], NULL, req_func, (void *)&reqs[i]))
        {
            cout << "Failed to create thread" << endl;
        }
        sleepTime += reqs[i].arrivalTime;
        sleep(sleepTime);
    }

    for (int i = 0; i < reqs.size(); i++)
    {
        pthread_join(tid[i], NULL);
    }
}

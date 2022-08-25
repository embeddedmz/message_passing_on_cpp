#include <iostream>

#include "BlockingCollection.h"
#include "cpptime.h"

using namespace code_machina;

int main()
{
    // A bounded collection. It can hold no more 
// than 100 items at once.
    BlockingCollection<int> collection(100);

    // a simple blocking consumer
    std::thread consumer_thread([&collection]() {

        while (!collection.is_completed())
        {
            int data;

            // take will block if there is no data to be taken
            auto status = collection.take(data);

            if (status == BlockingCollectionStatus::Ok)
            {
                std::cout << "Data: " << data << '\n';
            }

            // Status can also return BlockingCollectionStatus::Completed meaning take was called 
            // on a completed collection. Some other thread can call complete_adding after we pass 
            // the is_completed check but before we call take. In this example, we can ignore that
            // status since the loop will break on the next iteration.
        }
        });

    // a simple blocking producer
    //std::thread producer_thread([&collection]() {

    for (auto data = 0; data < 1000; ++data)
    {
        // blocks if collection.size() == collection.bounded_capacity()
        collection.add(data);
    }

    // let consumer know we are done
    collection.complete_adding();
    //});

    

    consumer_thread.join();
}

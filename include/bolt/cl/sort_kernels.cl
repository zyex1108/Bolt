#pragma OPENCL EXTENSION cl_amd_printf : enable
//#pragma OPENCL EXTENSION cl_khr_fp64 : enable 
namespace bolt{
    namespace cl{
        template<typename T>
        struct greater
        {
            bool operator()(const T &lhs, const T &rhs) const  {return (lhs > rhs);}
        };
        template<typename T>
        struct less 
        {
            bool operator()(const T &lhs, const T &rhs) const  {return (lhs < rhs);}
        };
};
};
//FIXME - this was added to support POD with bolt::cl::greater data types
template<typename T>
struct greater
{
    bool operator()(const T &lhs, const T &rhs) const  {return (lhs > rhs);}
};
template<typename T>
struct less 
{
    bool operator()(const T &lhs, const T &rhs) const  {return (lhs < rhs);}
};

template <typename T, typename Compare>
kernel
void sortTemplate(global T * theArray, 
                 const uint stage, 
                 const uint passOfStage,
                 global Compare *userComp)
{
    uint threadId = get_global_id(0);
    uint pairDistance = 1 << (stage - passOfStage);
    uint blockWidth   = 2 * pairDistance;
    uint temp;
    uint leftId = (threadId % pairDistance) 
                   + (threadId / pairDistance) * blockWidth;
    bool compareResult;
    
    uint rightId = leftId + pairDistance;
    
    T greater, lesser;
    T leftElement = theArray[leftId];
    T rightElement = theArray[rightId];
    
    uint sameDirectionBlockWidth = 1 << stage;
    
    if((threadId/sameDirectionBlockWidth) % 2 == 1)
    {
        temp = rightId;
        rightId = leftId;
        leftId = temp;
    }

    compareResult = (*userComp)(leftElement, rightElement);

    if(compareResult)
    {
        greater = rightElement;
        lesser  = leftElement;
    }
    else
    {
        greater = leftElement;
        lesser  = rightElement;
    }
    theArray[leftId]  = lesser;
    theArray[rightId] = greater;

}


template <typename T>
kernel
void sortTemplateBasic(global T * theArray, 
                 const uint stage, 
                 const uint passOfStage)
{
    uint threadId = get_global_id(0);
    uint pairDistance = 1 << (stage - passOfStage);
    uint blockWidth   = 2 * pairDistance;
    uint temp;
    uint leftId = (threadId % pairDistance) 
                   + (threadId / pairDistance) * blockWidth;
    bool compareResult;

    uint rightId = leftId + pairDistance;

    T greater, lesser;
    T leftElement = theArray[leftId];
    T rightElement = theArray[rightId];
    
    uint sameDirectionBlockWidth = 1 << stage;
    
    if((threadId/sameDirectionBlockWidth) % 2 == 1)
    {
        temp = rightId;
        rightId = leftId;
        leftId = temp;
    }

    compareResult = (leftElement < rightElement);
    if(compareResult)
    {
        greater = rightElement;
        lesser  = leftElement;
    }
    else
    {
        greater = leftElement;
        lesser  = rightElement;
    }
    theArray[leftId]  = lesser;
    theArray[rightId] = greater;
}

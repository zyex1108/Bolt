/***************************************************************************                                                                                     
*   Copyright 2012 Advanced Micro Devices, Inc.                                     
*                                                                                    
*   Licensed under the Apache License, Version 2.0 (the "License");   
*   you may not use this file except in compliance with the License.                 
*   You may obtain a copy of the License at                                          
*                                                                                    
*       http://www.apache.org/licenses/LICENSE-2.0                      
*                                                                                    
*   Unless required by applicable law or agreed to in writing, software              
*   distributed under the License is distributed on an "AS IS" BASIS,              
*   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.         
*   See the License for the specific language governing permissions and              
*   limitations under the License.                                                   

***************************************************************************/                                                                                     

#define TEST_DOUBLE 1
#define TEST_DEVICE_VECTOR 1
#define TEST_CPU_DEVICE 0
#define TEST_MULTICORE_TBB_SORT 1
#define GOOGLE_TEST 1
#if (GOOGLE_TEST == 1)
#include <gtest/gtest.h>
#include "common/stdafx.h"
#include "common/myocl.h"
#include "common/test_common.h"

#include <bolt/cl/sort.h>
#include <bolt/miniDump.h>
#include <bolt/unicode.h>


#include <boost/shared_array.hpp>
#include <array>
#include <algorithm>
/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Below are helper routines to compare the results of two arrays for googletest
//  They return an assertion object that googletest knows how to track
//This is a compare routine for naked pointers.

// UDD which contains four doubles
BOLT_FUNCTOR(uddtD4,
struct uddtD4
{
    double a;
    double b;
    double c;
    double d;

    bool operator==(const uddtD4& rhs) const
    {
        bool equal = true;
        double th = 0.0000000001;
        if (rhs.a < th && rhs.a > -th)
            equal = ( (1.0*a - rhs.a) < th && (1.0*a - rhs.a) > -th) ? equal : false;
        else
            equal = ( (1.0*a - rhs.a)/rhs.a < th && (1.0*a - rhs.a)/rhs.a > -th) ? equal : false;
        if (rhs.b < th && rhs.b > -th)
            equal = ( (1.0*b - rhs.b) < th && (1.0*b - rhs.b) > -th) ? equal : false;
        else
            equal = ( (1.0*b - rhs.b)/rhs.b < th && (1.0*b - rhs.b)/rhs.b > -th) ? equal : false;
        if (rhs.c < th && rhs.c > -th)
            equal = ( (1.0*c - rhs.c) < th && (1.0*c - rhs.c) > -th) ? equal : false;
        else
            equal = ( (1.0*c - rhs.c)/rhs.c < th && (1.0*c - rhs.c)/rhs.c > -th) ? equal : false;
        if (rhs.d < th && rhs.d > -th)
            equal = ( (1.0*d - rhs.d) < th && (1.0*d - rhs.d) > -th) ? equal : false;
        else
            equal = ( (1.0*d - rhs.d)/rhs.d < th && (1.0*d - rhs.d)/rhs.d > -th) ? equal : false;
        return equal;
    }
};
);

// Functor for UDD. Adds all four double elements and returns true if lhs_sum > rhs_sum
BOLT_FUNCTOR(AddD4,
struct AddD4
{
    bool operator()(const uddtD4 &lhs, const uddtD4 &rhs) const
    {

        if( ( lhs.a + lhs.b + lhs.c + lhs.d ) > ( rhs.a + rhs.b + rhs.c + rhs.d) )
            return true;
        return false;
    };
}; 
);
uddtD4 identityAddD4 = { 1.0, 1.0, 1.0, 1.0 };
uddtD4 initialAddD4  = { 1.00001, 1.000003, 1.0000005, 1.00000007 };
BOLT_CREATE_TYPENAME( bolt::cl::device_vector< uddtD4 >::iterator );

TEST(SortUDD, AddDouble4)
{
    //setup containers
    int length = (1<<8);
    bolt::cl::device_vector< uddtD4 > input(  length, initialAddD4,  CL_MEM_READ_WRITE, true  );
    std::vector< uddtD4 > refInput( length, initialAddD4 );

    // call sort
    AddD4 ad4gt;
    bolt::cl::sort(input.begin(),    input.end(), ad4gt);
    std::sort( refInput.begin(), refInput.end(), ad4gt );

    // compare results
    cmpArrays(refInput, input);
}

TEST(SortUDD, GPUAddDouble4)
{

    ::cl::Context myContext = bolt::cl::control::getDefault( ).context( );
    std::vector< cl::Device > devices = myContext.getInfo< CL_CONTEXT_DEVICES >();
    ::cl::CommandQueue myQueue( myContext, devices[ 0 ] );
    bolt::cl::control c_gpu( myQueue );  // construct control structure from the queue.
    //setup containers
    int length = (1<<8);
    bolt::cl::device_vector< uddtD4 > input(  length, initialAddD4,  CL_MEM_READ_WRITE, true  );
    std::vector< uddtD4 > refInput( length, initialAddD4 );

    // call sort
    AddD4 ad4gt;
    bolt::cl::sort(c_gpu, input.begin(), input.end(), ad4gt );
    std::sort( refInput.begin(), refInput.end(), ad4gt );

    // compare results
    cmpArrays(refInput, input);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Fixture classes are now defined to enable googletest to process type parameterized tests

//  This class creates a C++ 'TYPE' out of a size_t value
template< size_t N >
class TypeValue
{
public:
    static const size_t value = N;
};

//  Test fixture class, used for the Type-parameterized tests
//  Namely, the tests that use std::array and TYPED_TEST_P macros
template< typename ArrayTuple >
class SortArrayTest: public ::testing::Test
{
public:
    SortArrayTest( ): m_Errors( 0 )
    {}

    virtual void SetUp( )
    {
        std::generate(stdInput.begin(), stdInput.end(), rand);
        boltInput = stdInput;
    };

    virtual void TearDown( )
    {};

    virtual ~SortArrayTest( )
    {}

protected:
    typedef typename std::tuple_element< 0, ArrayTuple >::type ArrayType;
    static const size_t ArraySize = typename std::tuple_element< 1, ArrayTuple >::type::value;
    typename std::array< ArrayType, ArraySize > stdInput, boltInput;
    int m_Errors;
};

TYPED_TEST_CASE_P( SortArrayTest );

TYPED_TEST_P( SortArrayTest, Normal )
{
    typedef std::array< ArrayType, ArraySize > ArrayCont;

    //  Calling the actual functions under test
    std::sort( stdInput.begin( ), stdInput.end( ));
    bolt::cl::sort( boltInput.begin( ), boltInput.end( ) );

    ArrayCont::difference_type stdNumElements = std::distance( stdInput.begin( ), stdInput.end() );
    ArrayCont::difference_type boltNumElements = std::distance( boltInput.begin( ), boltInput.end() );

    //  Both collections should have the same number of elements
    EXPECT_EQ( stdNumElements, boltNumElements );

    //  Loop through the array and compare all the values with each other
    cmpStdArray< ArrayType, ArraySize >::cmpArrays( stdInput, boltInput );
    
}
#if (TEST_MULTICORE_TBB_SORT == 1)

TEST(MultiCoreCPU, MultiCoreAddDouble4)
{
    //setup containers
    int length = (1<<8);
    ::cl::Context myContext = bolt::cl::control::getDefault( ).context( );
    bolt::cl::control ctl = bolt::cl::control::getDefault( );
    ctl.forceRunMode(bolt::cl::control::MultiCoreCpu);
    bolt::cl::device_vector< uddtD4 > input(  length, initialAddD4, CL_MEM_READ_WRITE, true  );
    std::vector< uddtD4 > refInput( length, initialAddD4 );
    
    // call sort
    AddD4 ad4gt;
    bolt::cl::sort(input.begin(), input.end(), ad4gt);
    std::sort( refInput.begin(), refInput.end(), ad4gt );

    // compare results
    cmpArrays(refInput, input);
}

TEST( MultiCoreCPU, MultiCoreNormal )
{
    int length = 1098376;
    bolt::cl::device_vector< float > boltInput(  length, 0.0, CL_MEM_READ_WRITE, true  );
    std::vector< float > stdInput( length, 0.0 );

    ::cl::Context myContext = bolt::cl::control::getDefault( ).context( );
    bolt::cl::control ctl = bolt::cl::control::getDefault( );
    ctl.forceRunMode(bolt::cl::control::MultiCoreCpu);
    //  Calling the actual functions under test
    std::sort( stdInput.begin( ), stdInput.end( ));
    bolt::cl::sort( ctl, boltInput.begin( ), boltInput.end( ) );

    std::vector< float >::difference_type stdNumElements = std::distance( stdInput.begin( ), stdInput.end() );
    bolt::cl::device_vector< float >::difference_type boltNumElements = std::distance( boltInput.begin( ), boltInput.end() );

    //  Both collections should have the same number of elements
    EXPECT_EQ( stdNumElements, boltNumElements );

    //  Loop through the array and compare all the values with each other
    cmpArrays( stdInput, boltInput );
}
#endif

TYPED_TEST_P( SortArrayTest, GPU_DeviceNormal )
{
    //  The first time our routines get called, we compile the library kernels with a certain context
    //  OpenCL does not allow the context to change without a recompile of the kernel
    // MyOclContext oclgpu = initOcl(CL_DEVICE_TYPE_GPU, 0);
    //bolt::cl::control c_gpu(oclgpu._queue);  // construct control structure from the queue.

    //  Create a new command queue for a different device, but use the same context as was provided
    //  by the default control device
    ::cl::Context myContext = bolt::cl::control::getDefault( ).context( );
    std::vector< cl::Device > devices = myContext.getInfo< CL_CONTEXT_DEVICES >();
    ::cl::CommandQueue myQueue( myContext, devices[ 0 ] );
    bolt::cl::control c_gpu( myQueue );  // construct control structure from the queue.

    typedef std::array< ArrayType, ArraySize > ArrayCont;

    //  Calling the actual functions under test
    std::sort( stdInput.begin( ), stdInput.end( ));
    bolt::cl::sort( c_gpu, boltInput.begin( ), boltInput.end( ) );

    ArrayCont::difference_type stdNumElements = std::distance( stdInput.begin( ), stdInput.end() );
    ArrayCont::difference_type boltNumElements = std::distance( boltInput.begin( ), boltInput.end() );

    //  Both collections should have the same number of elements
    EXPECT_EQ( stdNumElements, boltNumElements );

    //  Loop through the array and compare all the values with each other
    cmpStdArray< ArrayType, ArraySize >::cmpArrays( stdInput, boltInput );
    // FIXME - releaseOcl(ocl);
}

#if (TEST_CPU_DEVICE == 1)
TYPED_TEST_P( SortArrayTest, CPU_DeviceNormal )
{
    typedef std::array< ArrayType, ArraySize > ArrayCont;

    MyOclContext oclcpu = initOcl(CL_DEVICE_TYPE_CPU, 0);
    bolt::cl::control c_cpu(oclcpu._queue);  // construct control structure from the queue.

    //  Calling the actual functions under test
    std::sort( stdInput.begin( ), stdInput.end( ));
    bolt::cl::sort( c_cpu, boltInput.begin( ), boltInput.end( ) );

    ArrayCont::difference_type stdNumElements = std::distance( stdInput.begin( ), stdInput.end() );
    ArrayCont::difference_type boltNumElements = std::distance( boltInput.begin( ), boltInput.end() );

    //  Both collections should have the same number of elements
    EXPECT_EQ( stdNumElements, boltNumElements );

    //  Loop through the array and compare all the values with each other
    cmpStdArray< ArrayType, ArraySize >::cmpArrays( stdInput, boltInput );
    // FIXME - releaseOcl(ocl);
}
#endif

TYPED_TEST_P( SortArrayTest, GreaterFunction )
{
    typedef std::array< ArrayType, ArraySize > ArrayCont;

    //  Calling the actual functions under test
    std::sort( stdInput.begin( ), stdInput.end( ), std::greater< ArrayType >() );
    
    bolt::cl::sort( boltInput.begin( ), boltInput.end( ), bolt::cl::greater< ArrayType >( ) );

    ArrayCont::difference_type stdNumElements = std::distance( stdInput.begin( ), stdInput.end() );
    ArrayCont::difference_type boltNumElements = std::distance( boltInput.begin( ), boltInput.end() );

    //  Both collections should have the same number of elements
    EXPECT_EQ( stdNumElements, boltNumElements );

    //  Loop through the array and compare all the values with each other
    cmpStdArray< ArrayType, ArraySize >::cmpArrays( stdInput, boltInput );
    
}

TYPED_TEST_P( SortArrayTest, GPU_DeviceGreaterFunction )
{
    typedef std::array< ArrayType, ArraySize > ArrayCont;
    //  The first time our routines get called, we compile the library kernels with a certain context
    //  OpenCL does not allow the context to change without a recompile of the kernel
    // MyOclContext oclgpu = initOcl(CL_DEVICE_TYPE_GPU, 0);
    //bolt::cl::control c_gpu(oclgpu._queue);  // construct control structure from the queue.

    //  Create a new command queue for a different device, but use the same context as was provided
    //  by the default control device
    ::cl::Context myContext = bolt::cl::control::getDefault( ).context( );
    std::vector< cl::Device > devices = myContext.getInfo< CL_CONTEXT_DEVICES >();
    ::cl::CommandQueue myQueue( myContext, devices[ 0 ] );
    bolt::cl::control c_gpu( myQueue );  // construct control structure from the queue.

    //  Calling the actual functions under test
    std::sort( stdInput.begin( ), stdInput.end( ), std::greater< ArrayType >( ));
    bolt::cl::sort( c_gpu, boltInput.begin( ), boltInput.end( ), bolt::cl::greater< ArrayType >( ) );

    ArrayCont::difference_type stdNumElements = std::distance( stdInput.begin( ), stdInput.end() );
    ArrayCont::difference_type boltNumElements = std::distance( boltInput.begin( ), boltInput.end() );

    //  Both collections should have the same number of elements
    EXPECT_EQ( stdNumElements, boltNumElements );

    //  Loop through the array and compare all the values with each other
    cmpStdArray< ArrayType, ArraySize >::cmpArrays( stdInput, boltInput );
    // FIXME - releaseOcl(ocl);
}
#if (TEST_CPU_DEVICE == 1)
TYPED_TEST_P( SortArrayTest, CPU_DeviceGreaterFunction )
{
    typedef std::array< ArrayType, ArraySize > ArrayCont;
    MyOclContext oclcpu = initOcl(CL_DEVICE_TYPE_CPU, 0);
    bolt::cl::control c_cpu(oclcpu._queue);  // construct control structure from the queue.

    //  Calling the actual functions under test
    std::sort( stdInput.begin( ), stdInput.end( ), std::greater< ArrayType >( ));
    bolt::cl::sort( c_cpu, boltInput.begin( ), boltInput.end( ), bolt::cl::greater< ArrayType >( ) );

    ArrayCont::difference_type stdNumElements = std::distance( stdInput.begin( ), stdInput.end() );
    ArrayCont::difference_type boltNumElements = std::distance( boltInput.begin( ), boltInput.end() );

    //  Both collections should have the same number of elements
    EXPECT_EQ( stdNumElements, boltNumElements );

    //  Loop through the array and compare all the values with each other
    cmpStdArray< ArrayType, ArraySize >::cmpArrays( stdInput, boltInput );
    // FIXME - releaseOcl(ocl);
}
#endif
TYPED_TEST_P( SortArrayTest, LessFunction )
{
    typedef std::array< ArrayType, ArraySize > ArrayCont;

    //  Calling the actual functions under test
    std::sort( stdInput.begin( ), stdInput.end( ), std::less<ArrayType>());
    bolt::cl::sort( boltInput.begin( ), boltInput.end( ), bolt::cl::less<ArrayType>() );

    ArrayCont::difference_type stdNumElements = std::distance( stdInput.begin( ), stdInput.end() );
    ArrayCont::difference_type boltNumElements = std::distance( boltInput.begin( ), boltInput.end() );

    //  Both collections should have the same number of elements
    EXPECT_EQ( stdNumElements, boltNumElements );

    //  Loop through the array and compare all the values with each other
    cmpStdArray< ArrayType, ArraySize >::cmpArrays( stdInput, boltInput );

}

TYPED_TEST_P( SortArrayTest, GPU_DeviceLessFunction )
{
    typedef std::array< ArrayType, ArraySize > ArrayCont;
    //  The first time our routines get called, we compile the library kernels with a certain context
    //  OpenCL does not allow the context to change without a recompile of the kernel
    // MyOclContext oclgpu = initOcl(CL_DEVICE_TYPE_GPU, 0);
    //bolt::cl::control c_gpu(oclgpu._queue);  // construct control structure from the queue.

    //  Create a new command queue for a different device, but use the same context as was provided
    //  by the default control device
    ::cl::Context myContext = bolt::cl::control::getDefault( ).context( );
    std::vector< cl::Device > devices = myContext.getInfo< CL_CONTEXT_DEVICES >();
    ::cl::CommandQueue myQueue( myContext, devices[ 0 ] ); 
    bolt::cl::control c_gpu( myQueue );  // construct control structure from the queue.
    //  Calling the actual functions under test
    std::sort( stdInput.begin( ), stdInput.end( ), std::less< ArrayType >( ));
    bolt::cl::sort( c_gpu, boltInput.begin( ), boltInput.end( ), bolt::cl::less< ArrayType >( ) );

    ArrayCont::difference_type stdNumElements = std::distance( stdInput.begin( ), stdInput.end() );
    ArrayCont::difference_type boltNumElements = std::distance( boltInput.begin( ), boltInput.end() );

    //  Both collections should have the same number of elements
    EXPECT_EQ( stdNumElements, boltNumElements );

    //  Loop through the array and compare all the values with each other
    cmpStdArray< ArrayType, ArraySize >::cmpArrays( stdInput, boltInput );
    // FIXME - releaseOcl(ocl);
}
#if (TEST_CPU_DEVICE == 1)
TYPED_TEST_P( SortArrayTest, CPU_DeviceLessFunction )
{
    typedef std::array< ArrayType, ArraySize > ArrayCont;
    MyOclContext oclcpu = initOcl(CL_DEVICE_TYPE_CPU, 0);
    bolt::cl::control c_cpu(oclcpu._queue);  // construct control structure from the queue.

    //  Calling the actual functions under test
    std::sort( stdInput.begin( ), stdInput.end( ), std::less< ArrayType >( ));
    bolt::cl::sort( c_cpu, boltInput.begin( ), boltInput.end( ), bolt::cl::less< ArrayType >( ) );

    ArrayCont::difference_type stdNumElements = std::distance( stdInput.begin( ), stdInput.end() );
    ArrayCont::difference_type boltNumElements = std::distance( boltInput.begin( ), boltInput.end() );

    //  Both collections should have the same number of elements
    EXPECT_EQ( stdNumElements, boltNumElements );

    //  Loop through the array and compare all the values with each other
    cmpStdArray< ArrayType, ArraySize >::cmpArrays( stdInput, boltInput );
    // FIXME - releaseOcl(ocl);
}
#endif

#if (TEST_CPU_DEVICE == 1)
REGISTER_TYPED_TEST_CASE_P( SortArrayTest, Normal, GPU_DeviceNormal, 
                                           GreaterFunction, GPU_DeviceGreaterFunction,
                                           LessFunction, GPU_DeviceLessFunction, CPU_DeviceNormal, CPU_DeviceGreaterFunction, CPU_DeviceLessFunction);
#else
REGISTER_TYPED_TEST_CASE_P( SortArrayTest, Normal, GPU_DeviceNormal, 
                                           GreaterFunction, GPU_DeviceGreaterFunction,
                                           LessFunction, GPU_DeviceLessFunction );
#endif


/////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Fixture classes are now defined to enable googletest to process value parameterized tests
//  ::testing::TestWithParam< int > means that GetParam( ) returns int values, which i use for array size

class SortIntegerVector: public ::testing::TestWithParam< int >
{
public:

    SortIntegerVector( ): stdInput( GetParam( ) ), boltInput( GetParam( ) )
    {
        std::generate(stdInput.begin(), stdInput.end(), rand);
        boltInput = stdInput;
    }

protected:
    std::vector< int > stdInput, boltInput;
};

//  ::testing::TestWithParam< int > means that GetParam( ) returns int values, which i use for array size
class SortFloatVector: public ::testing::TestWithParam< int >
{
public:
    SortFloatVector( ): stdInput( GetParam( ) ), boltInput( GetParam( ) )
    {
        std::generate(stdInput.begin(), stdInput.end(), rand);
        boltInput = stdInput;    
    }

protected:
    std::vector< float > stdInput, boltInput;
};

#if (TEST_DOUBLE == 1)
//  ::testing::TestWithParam< int > means that GetParam( ) returns int values, which i use for array size
class SortDoubleVector: public ::testing::TestWithParam< int >
{
public:
    SortDoubleVector( ): stdInput( GetParam( ) ), boltInput( GetParam( ) )
    {
        std::generate(stdInput.begin(), stdInput.end(), rand);
        boltInput = stdInput;    
    }

protected:
    std::vector< double > stdInput, boltInput;
};
#endif

//  ::testing::TestWithParam< int > means that GetParam( ) returns int values, which i use for array size
class SortIntegerDeviceVector: public ::testing::TestWithParam< int >
{
public:
    // Create an std and a bolt vector of requested size, and initialize all the elements to 1
    SortIntegerDeviceVector( ): stdInput( GetParam( ) ), boltInput( static_cast<size_t>( GetParam( ) ) )
    {
        std::generate(stdInput.begin(), stdInput.end(), rand);
        //boltInput = stdInput;      
        //FIXME - The above should work but the below loop is used. 
        for (int i=0; i< GetParam( ); i++)
        {
            boltInput[i] = stdInput[i];
        }
    }

protected:
    std::vector< int > stdInput;
    bolt::cl::device_vector< int > boltInput;
};

//  ::testing::TestWithParam< int > means that GetParam( ) returns int values, which i use for array size
/*class SortUDDDeviceVector: public ::testing::TestWithParam< int >
{
public:
    // Create an std and a bolt vector of requested size, and initialize all the elements to 1
    SortUDDDeviceVector( ): stdInput( GetParam( ) ), boltInput( static_cast<size_t>( GetParam( ) ) )
    {
        std::generate(stdInput.begin(), stdInput.end(), rand);
        //boltInput = stdInput;      
        //FIXME - The above should work but the below loop is used. 
        for (int i=0; i< GetParam( ); i++)
        {
            boltInput[i] = stdInput[i];
        }
    }

protected:
    std::vector< UDD > stdInput;
    bolt::cl::device_vector< UDD > boltInput;
};*/

//  ::testing::TestWithParam< int > means that GetParam( ) returns int values, which i use for array size
class SortFloatDeviceVector: public ::testing::TestWithParam< int >
{
public:
    // Create an std and a bolt vector of requested size, and initialize all the elements to 1
    SortFloatDeviceVector( ): stdInput( GetParam( ) ), boltInput( static_cast<size_t>( GetParam( ) ) )
    {
        std::generate(stdInput.begin(), stdInput.end(), rand);
        //boltInput = stdInput;      
        //FIXME - The above should work but the below loop is used. 
        for (int i=0; i< GetParam( ); i++)
        {
            boltInput[i] = stdInput[i];
        }

    }

protected:
    std::vector< float > stdInput;
    bolt::cl::device_vector< float > boltInput;
};

#if (TEST_DOUBLE == 1)
//  ::testing::TestWithParam< int > means that GetParam( ) returns int values, which i use for array size
class SortDoubleDeviceVector: public ::testing::TestWithParam< int >
{
public:
    // Create an std and a bolt vector of requested size, and initialize all the elements to 1
    SortDoubleDeviceVector( ): stdInput( GetParam( ) ), boltInput( static_cast<size_t>( GetParam( ) ) )
    {
        std::generate(stdInput.begin(), stdInput.end(), rand);
        //boltInput = stdInput;      
        //FIXME - The above should work but the below loop is used. 
        for (int i=0; i< GetParam( ); i++)
        {
            boltInput[i] = stdInput[i];
        }
    }

protected:
    std::vector< double > stdInput;
    bolt::cl::device_vector< double > boltInput;
};
#endif

//  ::testing::TestWithParam< int > means that GetParam( ) returns int values, which i use for array size
class SortIntegerNakedPointer: public ::testing::TestWithParam< int >
{
public:
    //  Create an std and a bolt vector of requested size, and initialize all the elements to 1
    SortIntegerNakedPointer( ): stdInput( new int[ GetParam( ) ] ), boltInput( new int[ GetParam( ) ] )
    {}

    virtual void SetUp( )
    {
        size_t size = GetParam( );

        std::generate(stdInput, stdInput + size, rand);
        for (int i = 0; i<size; i++)
        {
            boltInput[i] = stdInput[i];
        }
    };

    virtual void TearDown( )
    {
        delete [] stdInput;
        delete [] boltInput;
    };

protected:
     int* stdInput;
     int* boltInput;
};

//  ::testing::TestWithParam< int > means that GetParam( ) returns int values, which i use for array size
class SortFloatNakedPointer: public ::testing::TestWithParam< int >
{
public:
    //  Create an std and a bolt vector of requested size, and initialize all the elements to 1
    SortFloatNakedPointer( ): stdInput( new float[ GetParam( ) ] ), boltInput( new float[ GetParam( ) ] )
    {}

    virtual void SetUp( )
    {
        size_t size = GetParam( );

        std::generate(stdInput, stdInput + size, rand);
        for (int i = 0; i<size; i++)
        {
            boltInput[i] = stdInput[i];
        }
    };

    virtual void TearDown( )
    {
        delete [] stdInput;
        delete [] boltInput;
    };

protected:
     float* stdInput;
     float* boltInput;
};

#if (TEST_DOUBLE ==1 )
//  ::testing::TestWithParam< int > means that GetParam( ) returns int values, which i use for array size
class SortDoubleNakedPointer: public ::testing::TestWithParam< int >
{
public:
    //  Create an std and a bolt vector of requested size, and initialize all the elements to 1
    SortDoubleNakedPointer( ): stdInput( new double[ GetParam( ) ] ), boltInput( new double[ GetParam( ) ] )
    {}

    virtual void SetUp( )
    {
        size_t size = GetParam( );

        std::generate(stdInput, stdInput + size, rand);

        for( int i=0; i < size; i++ )
        {
            boltInput[ i ] = stdInput[ i ];
        }
    };

    virtual void TearDown( )
    {
        delete [] stdInput;
        delete [] boltInput;
    };

protected:
     double* stdInput;
     double* boltInput;
};
#endif

TEST_P( SortIntegerVector, Normal )
{
    //  Calling the actual functions under test
    std::sort( stdInput.begin( ), stdInput.end( ) );
    bolt::cl::sort( boltInput.begin( ), boltInput.end( ) );

    std::vector< int >::iterator::difference_type stdNumElements = std::distance( stdInput.begin( ), stdInput.end( ) );
    std::vector< int >::iterator::difference_type boltNumElements = std::distance( boltInput.begin( ), boltInput.end( ) );

    //  Both collections should have the same number of elements
    EXPECT_EQ( stdNumElements, boltNumElements );

    //  Loop through the array and compare all the values with each other
    cmpArrays( stdInput, boltInput );
}

// Come Back here
TEST_P( SortFloatVector, Normal )
{
    //  Calling the actual functions under test
    std::sort( stdInput.begin( ), stdInput.end( ) );
    bolt::cl::sort( boltInput.begin( ), boltInput.end( ) );

    std::vector< float >::iterator::difference_type stdNumElements = std::distance( stdInput.begin( ), stdInput.end( ) );
    std::vector< float >::iterator::difference_type boltNumElements = std::distance( boltInput.begin( ), boltInput.end( ) );

    //  Both collections should have the same number of elements
    EXPECT_EQ( stdNumElements, boltNumElements );

    //  Loop through the array and compare all the values with each other
    cmpArrays( stdInput, boltInput );
}
#if (TEST_DOUBLE == 1)
TEST_P( SortDoubleVector, Inplace )
{
    //  Calling the actual functions under test
    std::sort( stdInput.begin( ), stdInput.end( ) );
    bolt::cl::sort( boltInput.begin( ), boltInput.end( ) );

    std::vector< double >::iterator::difference_type stdNumElements = std::distance( stdInput.begin( ), stdInput.end( ) );
    std::vector< double >::iterator::difference_type boltNumElements = std::distance( boltInput.begin( ), boltInput.end( ) );

    //  Both collections should have the same number of elements
    EXPECT_EQ( stdNumElements, boltNumElements );

    //  Loop through the array and compare all the values with each other
    cmpArrays( stdInput, boltInput );
}
#endif
#if (TEST_DEVICE_VECTOR == 1)
TEST_P( SortIntegerDeviceVector, Inplace )
{
    //  Calling the actual functions under test
    std::sort( stdInput.begin( ), stdInput.end( ) );
    bolt::cl::sort( boltInput.begin( ), boltInput.end( ) );

    std::vector< int >::iterator::difference_type stdNumElements = std::distance( stdInput.begin( ), stdInput.end( ) );
    std::vector< int >::iterator::difference_type boltNumElements = std::distance( boltInput.begin( ), boltInput.end( ) );

    //  Both collections should have the same number of elements
    EXPECT_EQ( stdNumElements, boltNumElements );

    //  Loop through the array and compare all the values with each other
    cmpArrays( stdInput, boltInput );
}
TEST_P( SortFloatDeviceVector, Inplace )
{
    //  Calling the actual functions under test
    std::sort( stdInput.begin( ), stdInput.end( ) );
    bolt::cl::sort( boltInput.begin( ), boltInput.end( ) );

    std::vector< float >::iterator::difference_type stdNumElements = std::distance( stdInput.begin( ), stdInput.end( ) );
    std::vector< float >::iterator::difference_type boltNumElements = std::distance( boltInput.begin( ), boltInput.end( ) );

    //  Both collections should have the same number of elements
    EXPECT_EQ( stdNumElements, boltNumElements );

    //  Loop through the array and compare all the values with each other
    cmpArrays( stdInput, boltInput );
}
#if (TEST_DOUBLE == 1)
TEST_P( SortDoubleDeviceVector, Inplace )
{
    //  Calling the actual functions under test
    std::sort( stdInput.begin( ), stdInput.end( ) );
    bolt::cl::sort( boltInput.begin( ), boltInput.end( ) );

    std::vector< double >::iterator::difference_type stdNumElements = std::distance( stdInput.begin( ), stdInput.end( ) );
    std::vector< double >::iterator::difference_type boltNumElements = std::distance( boltInput.begin( ), boltInput.end( ) );

    //  Both collections should have the same number of elements
    EXPECT_EQ( stdNumElements, boltNumElements );

    //  Loop through the array and compare all the values with each other
    cmpArrays( stdInput, boltInput );
}
#endif
#endif

TEST_P( SortIntegerNakedPointer, Inplace )
{
    size_t endIndex = GetParam( );

    //  Calling the actual functions under test
    stdext::checked_array_iterator< int* > wrapStdInput( stdInput, endIndex );
    std::sort( wrapStdInput, wrapStdInput + endIndex );
    //std::sort( stdInput, stdInput + endIndex );

    stdext::checked_array_iterator< int* > wrapBoltInput( boltInput, endIndex );
    bolt::cl::sort( wrapBoltInput, wrapBoltInput + endIndex );
    //bolt::cl::sort( boltInput, boltInput + endIndex );

    //  Loop through the array and compare all the values with each other
    cmpArrays( stdInput, boltInput, endIndex );
}

TEST_P( SortFloatNakedPointer, Inplace )
{
    size_t endIndex = GetParam( );

    //  Calling the actual functions under test
    stdext::checked_array_iterator< float* > wrapStdInput( stdInput, endIndex );
    std::sort( wrapStdInput, wrapStdInput + endIndex );
    //std::sort( stdInput, stdInput + endIndex );

    stdext::checked_array_iterator< float* > wrapBoltInput( boltInput, endIndex );
    bolt::cl::sort( wrapBoltInput, wrapBoltInput + endIndex );
    //bolt::cl::sort( boltInput, boltInput + endIndex );

    //  Loop through the array and compare all the values with each other
    cmpArrays( stdInput, boltInput, endIndex );
}

#if (TEST_DOUBLE == 1)
TEST_P( SortDoubleNakedPointer, Inplace )
{
    size_t endIndex = GetParam( );

    //  Calling the actual functions under test
    stdext::checked_array_iterator< double* > wrapStdInput( stdInput, endIndex );
    std::sort( wrapStdInput, wrapStdInput + endIndex );
    //std::sort( stdInput, stdInput + endIndex );

    stdext::checked_array_iterator< double* > wrapBoltInput( boltInput, endIndex );
    bolt::cl::sort( wrapBoltInput, wrapBoltInput + endIndex );
    //bolt::cl::sort( boltInput, boltInput + endIndex );

    //  Loop through the array and compare all the values with each other
    cmpArrays( stdInput, boltInput, endIndex );
}
#endif
std::array<int, 15> TestValues = {2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,16384,32768};
//Test lots of consecutive numbers, but small range, suitable for integers because they overflow easier
INSTANTIATE_TEST_CASE_P( SortRange, SortIntegerVector, ::testing::Range( 0, 1024, 7 ) );
INSTANTIATE_TEST_CASE_P( SortValues, SortIntegerVector, ::testing::ValuesIn( TestValues.begin(), TestValues.end() ) );
INSTANTIATE_TEST_CASE_P( SortRange, SortFloatVector, ::testing::Range( 0, 1024, 3 ) );
INSTANTIATE_TEST_CASE_P( SortValues, SortFloatVector, ::testing::ValuesIn( TestValues.begin(), TestValues.end() ) );
#if (TEST_DOUBLE == 1)
INSTANTIATE_TEST_CASE_P( SortRange, SortDoubleVector, ::testing::Range( 0, 1024, 21 ) );
INSTANTIATE_TEST_CASE_P( SortValues, SortDoubleVector, ::testing::ValuesIn( TestValues.begin(), TestValues.end() ) );
#endif
INSTANTIATE_TEST_CASE_P( SortRange, SortIntegerDeviceVector, ::testing::Range( 0, 1024, 53 ) );
INSTANTIATE_TEST_CASE_P( SortValues, SortIntegerDeviceVector, ::testing::ValuesIn( TestValues.begin(), TestValues.end() ) );
INSTANTIATE_TEST_CASE_P( SortRange, SortFloatDeviceVector, ::testing::Range( 0, 1024, 53 ) );
INSTANTIATE_TEST_CASE_P( SortValues, SortFloatDeviceVector, ::testing::ValuesIn( TestValues.begin(), TestValues.end() ) );
#if (TEST_DOUBLE == 1)
INSTANTIATE_TEST_CASE_P( SortRange, SortDoubleDeviceVector, ::testing::Range( 0, 1024, 53 ) );
INSTANTIATE_TEST_CASE_P( SortValues, SortDoubleDeviceVector, ::testing::ValuesIn( TestValues.begin(), TestValues.end() ) );
#endif
INSTANTIATE_TEST_CASE_P( SortRange, SortIntegerNakedPointer, ::testing::Range( 0, 1024, 13) );
INSTANTIATE_TEST_CASE_P( SortValues, SortIntegerNakedPointer, ::testing::ValuesIn( TestValues.begin(), TestValues.end() ) );
INSTANTIATE_TEST_CASE_P( SortRange, SortFloatNakedPointer, ::testing::Range( 0, 1024, 13) );
INSTANTIATE_TEST_CASE_P( SortValues, SortFloatNakedPointer, ::testing::ValuesIn( TestValues.begin(), TestValues.end() ) );
#if (TEST_DOUBLE == 1)
INSTANTIATE_TEST_CASE_P( SortRange, SortDoubleNakedPointer, ::testing::Range( 0, 1024, 13) );
INSTANTIATE_TEST_CASE_P( Sort, SortDoubleNakedPointer, ::testing::ValuesIn( TestValues.begin(), TestValues.end() ) );
#endif

typedef ::testing::Types< 
    std::tuple< int, TypeValue< 1 > >,
    std::tuple< int, TypeValue< 31 > >,
    std::tuple< int, TypeValue< 32 > >,
    std::tuple< int, TypeValue< 63 > >,
    std::tuple< int, TypeValue< 64 > >,
    std::tuple< int, TypeValue< 127 > >,
    std::tuple< int, TypeValue< 128 > >,
    std::tuple< int, TypeValue< 129 > >,
    std::tuple< int, TypeValue< 1000 > >,
    std::tuple< int, TypeValue< 1053 > >,
    std::tuple< int, TypeValue< 4096 > >,
    std::tuple< int, TypeValue< 4097 > >,
    std::tuple< int, TypeValue< 65535 > >,
    std::tuple< int, TypeValue< 65536 > >
> IntegerTests;

typedef ::testing::Types< 
    std::tuple< unsigned int, TypeValue< 1 > >,
    std::tuple< unsigned int, TypeValue< 31 > >,
    std::tuple< unsigned int, TypeValue< 32 > >,
    std::tuple< unsigned int, TypeValue< 63 > >,
    std::tuple< unsigned int, TypeValue< 64 > >,
    std::tuple< unsigned int, TypeValue< 127 > >,
    std::tuple< unsigned int, TypeValue< 128 > >,
    std::tuple< unsigned int, TypeValue< 129 > >,
    std::tuple< unsigned int, TypeValue< 1000 > >,
    std::tuple< unsigned int, TypeValue< 1053 > >,
    std::tuple< unsigned int, TypeValue< 4096 > >,
    std::tuple< unsigned int, TypeValue< 4097 > >,
    std::tuple< unsigned int, TypeValue< 65535 > >,
    std::tuple< unsigned int, TypeValue< 65536 > >
> UnsignedIntegerTests;

typedef ::testing::Types< 
    std::tuple< float, TypeValue< 1 > >,
    std::tuple< float, TypeValue< 31 > >,
    std::tuple< float, TypeValue< 32 > >,
    std::tuple< float, TypeValue< 63 > >,
    std::tuple< float, TypeValue< 64 > >,
    std::tuple< float, TypeValue< 127 > >,
    std::tuple< float, TypeValue< 128 > >,
    std::tuple< float, TypeValue< 129 > >,
    std::tuple< float, TypeValue< 1000 > >,
    std::tuple< float, TypeValue< 1053 > >,
    std::tuple< float, TypeValue< 4096 > >,
    std::tuple< float, TypeValue< 4097 > >,
    std::tuple< float, TypeValue< 65535 > >,
    std::tuple< float, TypeValue< 65536 > >
> FloatTests;

#if (TEST_DOUBLE == 1)
typedef ::testing::Types< 
    std::tuple< double, TypeValue< 1 > >,
    std::tuple< double, TypeValue< 31 > >,
    std::tuple< double, TypeValue< 32 > >,
    std::tuple< double, TypeValue< 63 > >,
    std::tuple< double, TypeValue< 64 > >,
    std::tuple< double, TypeValue< 127 > >,
    std::tuple< double, TypeValue< 128 > >,
    std::tuple< double, TypeValue< 129 > >,
    std::tuple< double, TypeValue< 1000 > >,
    std::tuple< double, TypeValue< 1053 > >,
    std::tuple< double, TypeValue< 4096 > >,
    std::tuple< double, TypeValue< 4097 > >,
    std::tuple< double, TypeValue< 65535 > >,
    std::tuple< double, TypeValue< 65536 > >
> DoubleTests;
#endif 

/********* Test case to reproduce SuiCHi bugs ******************/
BOLT_FUNCTOR(UDD,
struct UDD { 
    int a; 
    int b;

    bool operator() (const UDD& lhs, const UDD& rhs) const{ 
        return ((lhs.a+lhs.b) > (rhs.a+rhs.b));
    } 
    bool operator < (const UDD& other) const { 
        return ((a+b) < (other.a+other.b));
    }
    bool operator > (const UDD& other) const { 
        return ((a+b) > (other.a+other.b));
    }
    bool operator == (const UDD& other) const { 
        return ((a+b) == (other.a+other.b));
    }
    UDD() 
        : a(0),b(0) { } 
    UDD(int _in) 
        : a(_in), b(_in +1)  { } 
}; 
);

BOLT_FUNCTOR(sortBy_UDD_a,
    struct sortBy_UDD_a {
        bool operator() (const UDD& a, const UDD& b) const
        { 
            return (a.a>b.a); 
        };
    };
);

BOLT_FUNCTOR(sortBy_UDD_b,
    struct sortBy_UDD_b {
        bool operator() (UDD& a, UDD& b) 
        { 
            return (a.b>b.b); 
        };
    };
);

BOLT_CREATE_TYPENAME(bolt::cl::less<UDD>);
BOLT_CREATE_TYPENAME(bolt::cl::greater<UDD>);

BOLT_CREATE_TYPENAME( bolt::cl::device_vector< UDD >::iterator );
BOLT_CREATE_CLCODE( bolt::cl::device_vector< UDD >::iterator, bolt::cl::deviceVectorIteratorTemplate );

template< typename ArrayTuple >
class SortUDDArrayTest: public ::testing::Test
{
public:
    SortUDDArrayTest( ): m_Errors( 0 )
    {}

    virtual void SetUp( )
    {
        std::generate(stdInput.begin(), stdInput.end(), rand);
        boltInput = stdInput;
    };

    virtual void TearDown( )
    {};

    virtual ~SortUDDArrayTest( )
    {}

protected:
    typedef typename std::tuple_element< 0, ArrayTuple >::type ArrayType;
    static const size_t ArraySize = typename std::tuple_element< 1, ArrayTuple >::type::value;
    typename std::array< ArrayType, ArraySize > stdInput, boltInput;
    int m_Errors;
};

TYPED_TEST_CASE_P( SortUDDArrayTest );

TYPED_TEST_P( SortUDDArrayTest, Normal )
{
    typedef std::array< ArrayType, ArraySize > ArrayCont;
    //  Calling the actual functions under test

    std::sort( stdInput.begin( ), stdInput.end( ), UDD() );
    bolt::cl::sort( boltInput.begin( ), boltInput.end( ), UDD() );

    ArrayCont::difference_type stdNumElements = std::distance( stdInput.begin( ), stdInput.end() );
    ArrayCont::difference_type boltNumElements = std::distance( boltInput.begin( ), boltInput.end() );
    //  Both collections should have the same number of elements
    EXPECT_EQ( stdNumElements, boltNumElements );
    //  Loop through the array and compare all the values with each other
    cmpStdArray< ArrayType, ArraySize >::cmpArrays( stdInput, boltInput );

    std::sort( stdInput.begin( ), stdInput.end( ), sortBy_UDD_a() );
    bolt::cl::sort( boltInput.begin( ), boltInput.end( ), sortBy_UDD_a() );

    stdNumElements = std::distance( stdInput.begin( ), stdInput.end() );
    boltNumElements = std::distance( boltInput.begin( ), boltInput.end() );
    //  Both collections should have the same number of elements
    EXPECT_EQ( stdNumElements, boltNumElements );
    //  Loop through the array and compare all the values with each other
    cmpStdArray< ArrayType, ArraySize >::cmpArrays( stdInput, boltInput );

}


typedef ::testing::Types< 
    std::tuple< UDD, TypeValue< 1 > >,
    std::tuple< UDD, TypeValue< 31 > >,
    std::tuple< UDD, TypeValue< 32 > >,
    std::tuple< UDD, TypeValue< 63 > >,
    std::tuple< UDD, TypeValue< 64 > >,
    std::tuple< UDD, TypeValue< 127 > >,
    std::tuple< UDD, TypeValue< 128 > >,
    std::tuple< UDD, TypeValue< 129 > >,
    std::tuple< UDD, TypeValue< 1000 > >,
    std::tuple< UDD, TypeValue< 1053 > >,
    std::tuple< UDD, TypeValue< 4096 > >,
    std::tuple< UDD, TypeValue< 4097 > >,
    std::tuple< UDD, TypeValue< 65535 > >,
    std::tuple< UDD, TypeValue< 65536 > >
> UDDTests;



INSTANTIATE_TYPED_TEST_CASE_P( Integer, SortArrayTest, IntegerTests );
INSTANTIATE_TYPED_TEST_CASE_P( UnsignedInteger, SortArrayTest, UnsignedIntegerTests );
INSTANTIATE_TYPED_TEST_CASE_P( Float, SortArrayTest, FloatTests );
#if (TEST_DOUBLE == 1)
INSTANTIATE_TYPED_TEST_CASE_P( Double, SortArrayTest, DoubleTests );
#endif 



REGISTER_TYPED_TEST_CASE_P( SortUDDArrayTest,  Normal);
INSTANTIATE_TYPED_TEST_CASE_P( UDDTest, SortUDDArrayTest, UDDTests );

int main(int argc, char* argv[])
{
    ::testing::InitGoogleTest( &argc, &argv[ 0 ] );

    //  Register our minidump generating logic
    bolt::miniDumpSingleton::enableMiniDumps( );

    int retVal = RUN_ALL_TESTS( );

    //  Reflection code to inspect how many tests failed in gTest
    ::testing::UnitTest& unitTest = *::testing::UnitTest::GetInstance( );

    unsigned int failedTests = 0;
    for( int i = 0; i < unitTest.total_test_case_count( ); ++i )
    {
        const ::testing::TestCase& testCase = *unitTest.GetTestCase( i );
        for( int j = 0; j < testCase.total_test_count( ); ++j )
        {
            const ::testing::TestInfo& testInfo = *testCase.GetTestInfo( j );
            if( testInfo.result( )->Failed( ) )
                ++failedTests;
        }
    }

    //  Print helpful message at termination if we detect errors, to help users figure out what to do next
    if( failedTests )
    {
        bolt::tout << _T( "\nFailed tests detected in test pass; please run test again with:" ) << std::endl;
        bolt::tout << _T( "\t--gtest_filter=<XXX> to select a specific failing test of interest" ) << std::endl;
        bolt::tout << _T( "\t--gtest_catch_exceptions=0 to generate minidump of failing test, or" ) << std::endl;
        bolt::tout << _T( "\t--gtest_break_on_failure to debug interactively with debugger" ) << std::endl;
        bolt::tout << _T( "\t    (only on googletest assertion failures, not SEH exceptions)" ) << std::endl;
    }
    std::cout << "Test Completed. Press Enter to exit.\n .... ";
    //getchar();
    return retVal;
}
#else
//BOLT Header files
#include "common/myocl.h"
#include <bolt/cl/clcode.h>
#include <bolt/cl/device_vector.h>
#include <bolt/cl/sort.h>
#include <bolt/cl/functional.h>
#include <bolt/cl/control.h>


//STD Header files
#include <iostream>
#include <algorithm>  // for testing against STL functions.
#include <vector>

template <typename T>
T random_gen()
{
    return (T)rand();
}

template <>
int random_gen<int>()
{

    int result = rand();
    static bool toggle = true;
    toggle = toggle?false:true;
    if (toggle)
            return result;
    else
            return -result;
}

template <>
unsigned int random_gen<unsigned int>()
{
    return rand();
}

// A Data structure defining a less than operator
BOLT_TEMPLATE_FUNCTOR4( MyType,  int, float, double, unsigned int,
template <typename T> 
struct MyType { 
    T a; 

    bool operator() (const MyType& lhs, const MyType& rhs) const { 
        return (lhs.a > rhs.a);
    } 
    bool operator < (const MyType& other) const { 
        return (a < other.a);
    }
    bool operator > (const MyType& other) const { 
        return (a > other.a);
    }
    MyType(const MyType &other)
        : a(other.a) { } 
    MyType() 
        : a(0) { } 
    MyType(T& _in) 
        : a(_in) { } 
}; 
);
BOLT_TEMPLATE_REGISTER_NEW_TYPE( bolt::cl::less, int, MyType< int> );
BOLT_TEMPLATE_REGISTER_NEW_TYPE( bolt::cl::less, int, MyType< float> );
BOLT_TEMPLATE_REGISTER_NEW_TYPE( bolt::cl::less, int, MyType< unsigned int> );
BOLT_TEMPLATE_REGISTER_NEW_TYPE( bolt::cl::less, int, MyType< double> );
BOLT_TEMPLATE_REGISTER_NEW_TYPE( bolt::cl::greater, int, MyType< int> );
BOLT_TEMPLATE_REGISTER_NEW_TYPE( bolt::cl::greater, int, MyType< float> );
BOLT_TEMPLATE_REGISTER_NEW_TYPE( bolt::cl::greater, int, MyType< unsigned int> );
BOLT_TEMPLATE_REGISTER_NEW_TYPE( bolt::cl::greater, int, MyType< double> );

// A Data structure defining a Functor
BOLT_TEMPLATE_FUNCTOR4( MyFunctor,  int, float, double, unsigned int,
	template <typename T>    
	struct MyFunctor{ 
		T a; 
		T b; 

		bool operator() (const MyFunctor& lhs, const MyFunctor& rhs) const { 
			return (lhs.a > rhs.a);
		} 
		bool operator < (const MyFunctor& other) const { 
			return (a < other.a);
		}
		bool operator > (const MyFunctor& other) const { 
			return (a > other.a);
		}
		MyFunctor(const MyFunctor &other) 
			: a(other.a), b(other.b) { } 
		MyFunctor() 
			: a(0), b(0) { } 
		MyFunctor(T& _in) 
			: a(_in), b(_in) { } 
	}; 
);


BOLT_TEMPLATE_REGISTER_NEW_TYPE( bolt::cl::less, int, MyFunctor< int> );
BOLT_TEMPLATE_REGISTER_NEW_TYPE( bolt::cl::less, int, MyFunctor< float> );
BOLT_TEMPLATE_REGISTER_NEW_TYPE( bolt::cl::less, int, MyFunctor< unsigned int> );
BOLT_TEMPLATE_REGISTER_NEW_TYPE( bolt::cl::less, int, MyFunctor< double> );
BOLT_TEMPLATE_REGISTER_NEW_TYPE( bolt::cl::greater, int, MyFunctor< int> );
BOLT_TEMPLATE_REGISTER_NEW_TYPE( bolt::cl::greater, int, MyFunctor< float> );
BOLT_TEMPLATE_REGISTER_NEW_TYPE( bolt::cl::greater, int, MyFunctor< unsigned int> );
BOLT_TEMPLATE_REGISTER_NEW_TYPE( bolt::cl::greater, int, MyFunctor< double> );


template <typename stdType>
void UserDefinedBoltFunctorSortTestOfLength(size_t length)
{
    typedef MyFunctor<stdType> myfunctor;

    std::vector<myfunctor> stdInput(length);
    std::vector<myfunctor> boltInput(length);

    size_t i;
    for (i=0;i<length;i++)
    {
        boltInput[i].a = (stdType)(i +2);
        stdInput[i].a  = (stdType)(i +2);
    }
    
    bolt::cl::sort(boltInput.begin(), boltInput.end(),bolt::cl::greater<MyFunctor<stdType>>());
    std::sort(stdInput.begin(), stdInput.end(),bolt::cl::greater<myfunctor>());

    for (i=0; i<length; i++)
    {
        if (stdInput[i].a == boltInput[i].a)
            continue;
        else
            break;
    }
    if (i==length)
        std::cout << "Test Passed" <<std::endl;
    else 
        std::cout << "Test Failed i = " << i <<std::endl;
}

template <typename stdType>
void UserDefinedFunctorSortTestOfLength(size_t length)
{
    typedef MyFunctor<stdType> myfunctor;

    std::vector<myfunctor> stdInput(length);
    std::vector<myfunctor> boltInput(length);

    size_t i;
    for (i=0;i<length;i++)
    {
        boltInput[i].a = (stdType)(i +2);
        stdInput[i].a  = (stdType)(i +2);
    }
    
    bolt::cl::sort(boltInput.begin(), boltInput.end(),myfunctor());
    std::sort(stdInput.begin(), stdInput.end(),myfunctor());

    for (i=0; i<length; i++)
    {
        if (stdInput[i].a == boltInput[i].a)
            continue;
        else
            break;
    }
    if (i==length)
        std::cout << "Test Passed" <<std::endl;
    else 
        std::cout << "Test Failed i = " << i <<std::endl;
}

template <typename stdType>
void UserDefinedObjectSortTestOfLength(size_t length)
{
    typedef MyType<stdType> mytype;

    std::vector<mytype> stdInput(length);
    std::vector<mytype> boltInput(length);

    size_t i;
    for (i=0;i<length;i++)
    {
        boltInput[i].a = random_gen<stdType>();
        stdInput[i].a  = boltInput[i].a;
    }
    
    bolt::cl::sort(boltInput.begin(), boltInput.end(),bolt::cl::greater<mytype>());
    std::sort(stdInput.begin(), stdInput.end(),bolt::cl::greater<mytype>());
    for (i=0; i<length; i++)
    {
        if(stdInput[i].a == boltInput[i].a)
            continue;
        else
            break;
    }
    if (i==length)
        std::cout << "Test Passed" <<std::endl;
    else 
        std::cout << "Test Failed i = " << i <<std::endl;
}

template <typename T>
void BasicSortTestOfLength(size_t length)
{

    std::vector<T> stdInput(length);
    std::vector<T> boltInput(length);
    std::vector<T> stdBackup(length);
    std::generate (stdInput.begin(), stdInput.end(),rand);
    
    //Ascending Sort 
    size_t i;

    for (i=0;i<length;i++)
    {
        boltInput[i]= random_gen<T>();
        stdInput[i] = boltInput[i];
    }
    stdBackup = stdInput;
    
    bolt::cl::sort(boltInput.begin(), boltInput.end());
    std::sort(stdInput.begin(), stdInput.end());

    for (i=0; i<length; i++)
    {
        if(stdInput[i] == boltInput[i])
            continue;
        else
            break;
    }
    if (i==length)
    {
        std::cout << "\nTest Passed - Ascending" <<std::endl;
    }
    else 
    {
        std::cout << "\nTest Failed  - Ascending i = " << i <<std::endl;
        for (int j=0;j<256;j++)
        {
            if((i+j)<0 || (i+j)>=length)
                std::cout << "Out of Index\n";
            else
                printf("%5x -- %8x -- %8x\n",(i+j),stdInput[i+j],boltInput[i+j]);
        }
    }


#if 1
    //Descending Sort 
    stdInput = stdBackup;
    for (i=0;i<length;i++)
    {
        boltInput[i]= (T)(stdInput[i]);
    }


    bolt::cl::sort(boltInput.begin(), boltInput.end(), bolt::cl::greater<T>());
    std::sort(stdInput.begin(), stdInput.end(), bolt::cl::greater<T>());

    for (i=0; i<length; i++)
    {
        if(stdInput[i] == boltInput[i])
            continue;
        else
            break;
    }
    if (i==length)
    {
        std::cout << "\nTest Passed - Descending" <<std::endl;
    }
    else 
    {
        std::cout << "\nTest Failed - Descending i = " << i <<std::endl;
        for (int j=0;j<256;j++)
        {
            if((i+j)<0 || (i+j)>=length)
                std::cout << "Out of Index\n";
            else
                printf("%5x -- %8x -- %8x\n",(i+j),stdInput[i+j],boltInput[i+j]);
        }
    }
#endif

}

template <typename stdType>
void UDDSortTestOfLengthWithDeviceVector(size_t length)
{
    typedef MyType<stdType> mytype;
    std::vector<mytype> stdInput(length);
    bolt::cl::device_vector<mytype> boltInput(length);

    size_t i;
    for (i=0;i<length;i++)
    {
        //FIX ME - This should work
        //boltInput[i].a = (stdType)(length - i +2);
        stdInput[i].a  = (stdType)(length - i +2);
    }
    
    bolt::cl::sort(boltInput.begin(), boltInput.end());
    std::sort(stdInput.begin(), stdInput.end());
    for (i=0; i<length; i++)
    {
         //FIX ME - stdInput should be changed to boltInput
        if(stdInput[i].a == stdInput[i].a)
            continue;
        else
            break;
    }
    if (i==length)
        std::cout << "Test Passed" <<std::endl;
    else 
        std::cout << "Test Failed i = " << i <<std::endl;
}

template <typename stdType>
void UDDSortTestWithBoltFunctorOfLengthWithDeviceVector(size_t length)
{
    typedef MyType<stdType> mytype;
    std::vector<mytype> stdInput(length);
    bolt::cl::device_vector<mytype> boltInput(length);

    size_t i;
    for (i=0;i<length;i++)
    {
        //FIX ME - This should work        
        //boltInput[i].a = (stdType)(length - i +2);
        stdInput[i].a  = (stdType)(length - i +2);
    }
    
    bolt::cl::sort(boltInput.begin(), boltInput.end(),bolt::cl::less<mytype>());
    std::sort(stdInput.begin(), stdInput.end(),bolt::cl::less<mytype>());
    for (i=0; i<length; i++)
    {
         //FIX ME - stdInput should be changed to boltInput
        if(stdInput[i].a == stdInput[i].a)
            continue;
        else
            break;
    }
    if (i==length)
        std::cout << "Test Passed" <<std::endl;
    else 
        std::cout << "Test Failed i = " << i <<std::endl;
}
/*
void TestWithBoltControl(int length)
{

    MyOclContext ocl = initOcl(CL_DEVICE_TYPE_CPU, 0);
    bolt::cl::control c(ocl._queue);  // construct control structure from the queue.
    //c.debug(bolt::cl::control::debug::Compile + bolt::cl::control::debug::SaveCompilerTemps);
    typedef MyFunctor<int> myfunctor;
    typedef MyType<int> mytype;

    std::vector<int> boltInput(length);
    std::vector<int> stdInput(length);
    std::vector<myfunctor> myFunctorBoltInput(length);
    std::vector<mytype> myTypeBoltInput(length);
    //bolt::cl::device_vector<int> dvInput(c,length);
    size_t i;

    for (i=0;i<length;i++)
    {
        boltInput[i]= (int)(length - i +2);
        stdInput[i]= (int)(length - i +2);
    }
    for (i=0;i<length;i++)
    {
        myFunctorBoltInput[i].a= (int)(length - i +2);
        myTypeBoltInput[i].a= (int)(length - i +2);
    }

    //BasicSortTestOfLengthWithDeviceVector
    bolt::cl::device_vector<int> dvInput( boltInput.begin(), boltInput.end(), CL_MEM_USE_HOST_PTR | CL_MEM_READ_WRITE, c);
    bolt::cl::sort(c, dvInput.begin(), dvInput.end());

    std::sort(stdInput.begin(),stdInput.end());
    for (i=0;i<length;i++)
    {
        if(dvInput[i] == stdInput[i])
        {
            continue;
        }
        else
            break;
    }
    if(i==length)
        std::cout << "Test Passed\n";
    else
        std::cout << "Test Failed. i = "<< i << std::endl;
    //Device Vector with greater functor
    bolt::cl::sort(c, dvInput.begin(), dvInput.end(),bolt::cl::greater<int>());
    //UserDefinedBoltFunctorSortTestOfLength
    bolt::cl::sort(c, myFunctorBoltInput.begin(), myFunctorBoltInput.end(),bolt::cl::greater<myfunctor>());
    //UserDefinedBoltFunctorSortTestOfLength
    bolt::cl::sort(c, myTypeBoltInput.begin(), myTypeBoltInput.end(),bolt::cl::greater<mytype>());
    return;
}
*/
int main(int argc, char* argv[])
{

#define TEST_ALL 1
#if (TEST_ALL == 1)
    //UDDSortTestOfLengthWithDeviceVector<int>(256);
    BasicSortTestOfLength<int>(256/*2097152/*131072/*16777216/*33554432/*atoi(argv[1])*/);
    BasicSortTestOfLength<int>(4096);
    BasicSortTestOfLength<int>(4096+64);
    BasicSortTestOfLength<int>(2097152);
    BasicSortTestOfLength<int>(2097152+67);
    BasicSortTestOfLength<int>(131072);
    BasicSortTestOfLength<int>(512);
    BasicSortTestOfLength<int>(1024);
    BasicSortTestOfLength<int>(2048);
    BasicSortTestOfLength<int>(2560);
#endif
#if (TEST_ALL == 1)
    BasicSortTestOfLength<unsigned int>(256/*2097152/*131072/*16777216/*33554432/*atoi(argv[1])*/);
    BasicSortTestOfLength<unsigned int>(4096);
    BasicSortTestOfLength<unsigned int>(4096+97);
    BasicSortTestOfLength<unsigned int>(2097152);
    BasicSortTestOfLength<unsigned int>(131072);
    BasicSortTestOfLength<unsigned int>(16777472); // 2^24 + 256
    BasicSortTestOfLength<unsigned int>(16777472+77);
    BasicSortTestOfLength<unsigned int>(2048);
    BasicSortTestOfLength<unsigned int>(2560);
    BasicSortTestOfLength<unsigned int>(2560+64);
    BasicSortTestOfLength<float>(256);
    BasicSortTestOfLength<float>(512);
    BasicSortTestOfLength<float>(1024);
    BasicSortTestOfLength<float>(2048);
    BasicSortTestOfLength<float>(1048576);
    BasicSortTestOfLength<float>(1048576-897);

#if (TEST_DOUBLE == 1) 
    BasicSortTestOfLength<double>(256);
    BasicSortTestOfLength<double>(512);
    BasicSortTestOfLength<double>(1024);
    BasicSortTestOfLength<double>(2048);
    BasicSortTestOfLength<double>(1048576);
    BasicSortTestOfLength<double>(1048576-897);
#endif
#endif
#if (TEST_ALL == 1)

    //Test the non Power Of 2 Buffer size 
    //The following two commented codes does not run. It will throw and cl::Error exception
    UserDefinedBoltFunctorSortTestOfLength<int>(256);
	UserDefinedBoltFunctorSortTestOfLength<float>(256);
    UserDefinedFunctorSortTestOfLength<int>(254);
    UserDefinedObjectSortTestOfLength<int>(254);
    UDDSortTestOfLengthWithDeviceVector<int>(256);
    UDDSortTestWithBoltFunctorOfLengthWithDeviceVector<int>(256);
#endif

//#define TEST_DOUBLE 1


#if (TEST_ALL == 1)
    std::cout << "Testing UserDefinedBoltFunctorSortTestOfLength\n";
    UserDefinedBoltFunctorSortTestOfLength<int>(8);   
    UserDefinedBoltFunctorSortTestOfLength<int>(16);    
    UserDefinedBoltFunctorSortTestOfLength<int>(32);    
    UserDefinedBoltFunctorSortTestOfLength<int>(64);    
    UserDefinedBoltFunctorSortTestOfLength<int>(128); 
    UserDefinedBoltFunctorSortTestOfLength<int>(256);   
    UserDefinedBoltFunctorSortTestOfLength<int>(512);    
    UserDefinedBoltFunctorSortTestOfLength<int>(1024);    
    UserDefinedBoltFunctorSortTestOfLength<int>(2048);    
    UserDefinedBoltFunctorSortTestOfLength<int>(1048576); 
    UserDefinedBoltFunctorSortTestOfLength<int>(1048576+75); 
    UserDefinedBoltFunctorSortTestOfLength<float>(8);   
    UserDefinedBoltFunctorSortTestOfLength<float>(16);    
    UserDefinedBoltFunctorSortTestOfLength<float>(32);    
    UserDefinedBoltFunctorSortTestOfLength<float>(64);    
    UserDefinedBoltFunctorSortTestOfLength<float>(128); 
    UserDefinedBoltFunctorSortTestOfLength<float>(256);
    UserDefinedBoltFunctorSortTestOfLength<float>(512);
    UserDefinedBoltFunctorSortTestOfLength<float>(1024);
    UserDefinedBoltFunctorSortTestOfLength<float>(2048);
    UserDefinedBoltFunctorSortTestOfLength<float>(1048576);
    UserDefinedBoltFunctorSortTestOfLength<float>(1048576+75);
#if (TEST_DOUBLE == 1)
    UserDefinedBoltFunctorSortTestOfLength<double>(8);   
    UserDefinedBoltFunctorSortTestOfLength<double>(16);    
    UserDefinedBoltFunctorSortTestOfLength<double>(32);    
    UserDefinedBoltFunctorSortTestOfLength<double>(64);    
    UserDefinedBoltFunctorSortTestOfLength<double>(128); 
    UserDefinedBoltFunctorSortTestOfLength<double>(256);
    UserDefinedBoltFunctorSortTestOfLength<double>(512);
    UserDefinedBoltFunctorSortTestOfLength<double>(1024);
    UserDefinedBoltFunctorSortTestOfLength<double>(2048);
    UserDefinedBoltFunctorSortTestOfLength<double>(1048576); 
    UserDefinedBoltFunctorSortTestOfLength<double>(1048576+75); 
#endif
#endif

#if (TEST_ALL == 1)
    std::cout << "Testing UserDefinedFunctorSortTestOfLength\n";
    UserDefinedFunctorSortTestOfLength<int>(8);   
    UserDefinedFunctorSortTestOfLength<int>(16);    
    UserDefinedFunctorSortTestOfLength<int>(32);    
    UserDefinedFunctorSortTestOfLength<int>(64);    
    UserDefinedFunctorSortTestOfLength<int>(128); 
    UserDefinedFunctorSortTestOfLength<int>(256);   
    UserDefinedFunctorSortTestOfLength<int>(512);    
    UserDefinedFunctorSortTestOfLength<int>(1024);    
    UserDefinedFunctorSortTestOfLength<int>(2048);    
    UserDefinedFunctorSortTestOfLength<int>(1048576); 
    UserDefinedFunctorSortTestOfLength<int>(1048576+75); 
    UserDefinedFunctorSortTestOfLength<float>(8);   
    UserDefinedFunctorSortTestOfLength<float>(16);    
    UserDefinedFunctorSortTestOfLength<float>(32);    
    UserDefinedFunctorSortTestOfLength<float>(64);    
    UserDefinedFunctorSortTestOfLength<float>(128); 
    UserDefinedFunctorSortTestOfLength<float>(256);   
    UserDefinedFunctorSortTestOfLength<float>(512);
    UserDefinedFunctorSortTestOfLength<float>(1024); 
    UserDefinedFunctorSortTestOfLength<float>(2048);
    UserDefinedFunctorSortTestOfLength<float>(1048576); 
    UserDefinedFunctorSortTestOfLength<float>(1048576+75); 

#if (TEST_DOUBLE == 0)     
    UserDefinedFunctorSortTestOfLength<double>(8);   
    UserDefinedFunctorSortTestOfLength<double>(16);    
    UserDefinedFunctorSortTestOfLength<double>(32);    
    UserDefinedFunctorSortTestOfLength<double>(64);    
    UserDefinedFunctorSortTestOfLength<double>(128); 
    UserDefinedFunctorSortTestOfLength<double>(256);   
    UserDefinedFunctorSortTestOfLength<double>(512);    
    UserDefinedFunctorSortTestOfLength<double>(1024);    
    UserDefinedFunctorSortTestOfLength<double>(2048);  
    UserDefinedFunctorSortTestOfLength<double>(1048576); 
    UserDefinedFunctorSortTestOfLength<double>(1048576+75); 
#endif
#endif


#if (TEST_ALL == 1)
    std::cout << "Testing UserDefinedObjectSortTestOfLength\n";
    UserDefinedObjectSortTestOfLength<int>(8);   
    UserDefinedObjectSortTestOfLength<int>(16);    
    UserDefinedObjectSortTestOfLength<int>(32);    
    UserDefinedObjectSortTestOfLength<int>(64);    
    UserDefinedObjectSortTestOfLength<int>(128); 
    UserDefinedObjectSortTestOfLength<int>(256);   
    UserDefinedObjectSortTestOfLength<int>(512);    
    UserDefinedObjectSortTestOfLength<int>(1024);    
    UserDefinedObjectSortTestOfLength<int>(2048);    
    UserDefinedObjectSortTestOfLength<int>(1048576); 
    UserDefinedObjectSortTestOfLength<int>(1048576+95); 
    UserDefinedObjectSortTestOfLength<float>(8);   
    UserDefinedObjectSortTestOfLength<float>(16);    
    UserDefinedObjectSortTestOfLength<float>(32);    
    UserDefinedObjectSortTestOfLength<float>(64);    
    UserDefinedObjectSortTestOfLength<float>(128); 
    UserDefinedObjectSortTestOfLength<float>(256);   
    UserDefinedObjectSortTestOfLength<float>(512);    
    UserDefinedObjectSortTestOfLength<float>(1024);    
    UserDefinedObjectSortTestOfLength<float>(2048);    
    UserDefinedObjectSortTestOfLength<float>(1048576);
    UserDefinedObjectSortTestOfLength<float>(1048576-31);
#if (TEST_DOUBLE == 1) 
    UserDefinedObjectSortTestOfLength<double>(8);   
    UserDefinedObjectSortTestOfLength<double>(16);    
    UserDefinedObjectSortTestOfLength<double>(32);    
    UserDefinedObjectSortTestOfLength<double>(64);    
    UserDefinedObjectSortTestOfLength<double>(128); 
    UserDefinedObjectSortTestOfLength<double>(256);   
    UserDefinedObjectSortTestOfLength<double>(512);    
    UserDefinedObjectSortTestOfLength<double>(1024);    
    UserDefinedObjectSortTestOfLength<double>(2048);    
    UserDefinedObjectSortTestOfLength<double>(1048576);
    UserDefinedObjectSortTestOfLength<double>(1048576-129);
#endif
#endif

    std::cout << "Test Completed" << std::endl; 
    return 0;
    getchar();
    return 0;
}
#endif

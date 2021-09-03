#define DEBUG_LEVEL 0

#include <cmath>
#include <iostream>
#include <fstream>
#include <random>
#include <functional>

typedef float (*activation_fn) (float);
typedef float* (*output_fn) (float[], size_t);
typedef float (*loss_fn) (float[], float[], size_t);
typedef float* (*loss_gradient) (float[], float[], size_t);

int Kronecker (int i, int j)
{
    return (i == j);
};

float* Identity (float x [], size_t n) 
{
    return x;
};

float Identity (float x)
{
    return x;
};

float ReLU (float x) 
{
    return (x > 0) ? x : 0;
};

float Heaviside (float x, float a) 
{
    return (x + a > 0) ? 1 : 0;
};

float Step (float x) 
{
    return (x > 0) ? 1 : 0;
};

float Sigmoid (float x) 
{
    return 1 / (1 + exp (-x));

};

float Max (float x [], size_t n)
{
    float max = x [0];

    for (int i = 1; i < n; i++)
    {
        if (x [i] > max)
        {
            max = x [i];
        };
    };

    return max;
};

float* Softmax (float x [], size_t n) 
{
    float total = 0.0;
    float stability = - Max (x, n);

    for (int i = 0; i < n; i++)
    {
        total += exp (x [i] + stability);
    };

    float* g = new float [n];

    for (int i = 0; i < n; i++)
    {
        g [i] = exp (x [i] + stability) / total;
    };

    return g;
};

float Softplus (float x);

float GELU (float x);

float ELU (float x);

float Gaussian (float x);


float WeightedSum (float values[], float weights [], float bias, size_t length) 
{
    float accumulated = bias;

    for (int i = 0; i < length; i++) 
    {
        accumulated += values [i] * weights [i];
    };

    return accumulated;
};


float MeanSquaredError (float output [], float expected [], size_t n)
{
    float total = 0.0;

    for (int i = 0; i < n; i++)
    {
        float difference = expected [i] - output [i];
        total += pow (difference, 2);
    };


    return total / n;
};

float* MeanSquaredErrorGradient (float output [], float expected [], size_t n) 
{
    float* g = new float [n];

    for (int i = 0; i < n; i++)
    {
        g [i] = - 2 * (expected [i] - output [i]) / n;
    };

    return g;
};

float CrossEntropy (float output [], float expected [], size_t n)
{
    float total = 0.0;
    float epsilon = 0.01;

    for (int i = 0; i < n; i++)
    {
        total += expected [i] * log (output [i] + epsilon);
    };


    return -total;
};

float* CrossEntropyGradient (float output [], float expected [], size_t n) 
{
    float* g = new float [n];

    for (int i = 0; i < n; i++)
    {
        g [i] = output [i] - expected [i];
    };

    return g;
};

struct Random 
{
    std::random_device rd;
    std::mt19937 generator;
    std::normal_distribution <float> distribution [];

    Random (size_t dim [], size_t depth) : generator (rd ()) 
    {
        for (int i = 0; i < depth; i++)
        {
            distribution [i] = std::normal_distribution <float> (0.0, ((float)1) / (float)(dim [i]));
        };
    };
    Random (size_t dim [], size_t depth, int seed) : generator (seed) 
    {
        for (int i = 0; i < depth; i++)
        {
            distribution [i] = std::normal_distribution <float> (0.0, ((float)1) / (float)(dim [i]));
        };
    };

    float RandomWeight (int i) 
    {
        return distribution [i] (generator);
    };
};


struct Layer
{
    float** weights;
    float* biases;
    struct { size_t M, N; } size;

    float* activations;
    float* x;

    activation_fn fn;
    activation_fn fn_prime;
    

    // Constructor
    Layer 
    (
        float** p, float* b, 
        size_t M, size_t N, 
        activation_fn f, activation_fn f_prime, 
        Random* r, int layer_depth
    ) 
        : fn (f), fn_prime (f_prime)
    {
        size.M = M;
        size.N = N;

        activations = new float [M]();
        x = new float [M]();
        biases = new float [M];

        weights = new float* [M];
        weights [0] = new float [M * N];
        for (int i = 1; i < M; i++)
        {
            weights [i] = weights [i - 1] + N; 
        };

        if (p == nullptr) 
        {
            for (int i = 0; i < M * N; i++)
            {
                weights [0][i] = r -> RandomWeight (layer_depth);
            };
        }
        else 
        {
            for (int i = 0; i < M; i++)
                for (int j = 0; j < N; j++)
                    weights [0][i * N + j] = p [i][j];
        };

        if (b == nullptr)
        {
            for (int i = 0; i < M; i++)
            {
                biases [i] = 0.0;
            };
        }
        else
        {
            biases = b;
        };
    };

    Layer () {};

    Layer (const Layer &l) {
        std::cout << "Copying Layer! " << std::endl;
    };

    ~Layer ()
    {
        delete [] weights [0];
        delete [] weights;
    };

    void SetActivations (float input [])
    {
        size_t M = size.M;
        size_t N = size.N;

        for (int i = 0; i < M; i++) 
        {
            x [i] = WeightedSum (input, weights [i], biases [i], N);
            activations [i] = fn (x [i]);
        };
    };
};

template <size_t depth>
struct Network 
{
    Layer* layers [depth];
    size_t* dimensions;
    float* output;

    output_fn OutputFunction;
    loss_fn LossFunction;
    loss_gradient LossGradient;

    float regularisation_factor;
    float learning_rate;

    Random* r;

    float** weight_gradients [depth];
    float* bias_gradients [depth];


    // Constructor
    Network 
    (
        size_t dimensions [depth + 1], 

        activation_fn functions [depth], 
        activation_fn derivatives [depth], 

        output_fn OutputFunction = Identity, 

        loss_fn LossFunction = CrossEntropy, 
        loss_gradient LossGradient = CrossEntropyGradient,
        float regularisation_factor = 0.0,
        float learning_rate = 0.01
    ) 

        // Initialisation List
        :
        dimensions (dimensions), 
        OutputFunction (OutputFunction), 
        LossFunction (LossFunction), 
        LossGradient (LossGradient), 
        regularisation_factor (regularisation_factor),
        learning_rate (learning_rate)

    // Constructor Body
    {
        output = new float [dimensions [depth]];
        r = new Random (dimensions, depth, 1000);

        for (int i = 0; i < depth; i++) 
        {
            size_t M = dimensions [i + 1];
            size_t N = dimensions [i];

            activation_fn f = functions [i];
            activation_fn f_prime = derivatives [i];

            float** w = new float* [M];
            for (int j = 0; j < M; j++)
            {
                w [j] = new float [N];
            };

            float* b = new float [M];

            weight_gradients [i] = w;
            bias_gradients [i] = b;

            layers [i] = new Layer (nullptr, nullptr, M, N, f, f_prime, r, i);
        };
    };

    ~Network ()
    {
        for (int i = 0; i < depth; i++)
        {
            delete layers [i];
        };
    };

    float* Propagate (float input []) 
    {
        for (int i = 0; i < depth; i++) 
        {
            Layer* l = layers [i];
            l -> SetActivations (input);

            input = l -> activations;
        };

        Layer* l = layers [depth - 1];
        size_t M = l -> size.M;
        float* x = OutputFunction (l -> activations, M);

        for (int i = 0; i < M; i++)
        {
            output [i] = x [i];
        };

        return output;
    };

    void PrintLayer (Layer* l) 
    {
        if (l == nullptr)
        {
            l = layers [depth - 1];
        }

        size_t M = l -> size.M;

        for (int i = 0; i < M; i++)
        {
            std::cout << l -> activations [i] << "  ";
        };

        std::cout << std::endl;
    };

    void PrintOutput ()
    {
        std::cout << "Output: ";

        for (int i = 0; i < dimensions [depth]; i++)
        {
            std::cout << output [i] << " ";
        };
        std::cout << std::endl;
    };

    void PrintAllLayers () 
    {
        for (int i = 0; i < depth; i++)
        {
            std::cout << std::endl << i << std::endl;
            PrintLayer (layers [i]);
        };
    };

    void PrintWeights ()
    {
        for (int i = 0; i < depth; i++)
        {
            Layer* layer = layers [i];

            size_t M = layer -> size.M;
            size_t N = layer -> size.N;

            std::cout << std::endl << "layer: " << i << std::endl;
            std::cout << "weights: " << std::endl;

            for (int j = 0; j < M; j++)
            {
                std::cout << "    ";
                for (int k = 0; k < N; k++)
                {
                    std::cout << layer -> weights [j][k] << " ";
                };
                std::cout << std::endl;
            };

            std::cout << "biases: ";

            for (int j = 0; j < M; j++)
            {
                std::cout << layer -> biases [j]<< " ";
            };
            std::cout << std::endl;
        };
        std::cout << std::endl << std::endl;
    };

    float Regulariser () 
    {
        // float bias_sum = 0.0;
        float weight_sum = 0.0;

        for (int i = 0; i < depth; i++)
        {
            Layer* layer = layers [i];
            float** w = layer -> weights;
            // float* b = layer -> biases;
            

            size_t M = layer -> size.M;
            size_t N = layer -> size.N;

            for (int j = 0; j < M; j++)
            {
                // bias_sum += pow (b [j], 2);

                for (int k = 0; k < N; k++)
                {
                    weight_sum += pow (w [j][k], 2);
                };
            };
        };

        // return weight_sum + bias_sum;
        return weight_sum;
    };

    void RegulariserGradientBiases (Layer* layer, float* b) 
    {
        // float* biases = layer -> biases;
        size_t M = layer -> size.M;

        for (int i = 0; i < M; i++)
        {
            // b [i] = 2 * biases [i];
            b [i] = 0;
        };
    };

    void RegulariserGradientWeights (Layer* layer, float** w) 
    {
        float** weights = layer -> weights;

        size_t M = layer -> size.M;
        size_t N = layer -> size.N;

        for (int i = 0; i < M; i++)
        {
            for (int j = 0; j < N; j++)
            {
                w [i][j] = 2 * weights [i][j];
            };
        };
    };

    float Cost (float input [], float expected [], float regularisation = 0.0) 
    {
        float* output = Propagate (input);
        size_t n = dimensions [depth];
        float loss = LossFunction (output, expected, n);

        return loss + regularisation;
    };

    float* TrainBatch (float* input_set [], float* expected_set [], size_t batch_size)
    { 
        float* costs = new float [batch_size];

        for (int i = 0; i < batch_size; i++)
        {
            float regularisation = regularisation_factor * Regulariser ();
            costs [i] = Cost (input_set [i], expected_set [i], regularisation);

            BackPropagate (input_set [i], expected_set [i]);
            UpdateWeightsAndBiases ();
        };

        return costs;
    };

    void BackPropagate (float input [], float expected []) 
    {
        float* y = Propagate (input);
        size_t n = dimensions [depth];
        float* g = LossGradient (y, expected, n);

        // Debug Console Log
        #if DEBUG_LEVEL == 1
        size_t input_dimension = dimensions [0];
        std::cout << std::endl << "input: ";
        for (int i = 0; i < input_dimension; i++) 
        {
            std::cout << input [i] << " ";
        };
        std::cout << std::endl;

        PrintOutput ();

        std::cout << "expected: ";
        for (int i = 0; i < n; i++) 
        {
            std::cout << expected [i] << " ";
        };
        std::cout << std::endl;
        #endif 

        // Iterate throug layers and calculate gradient
        for (int i = depth - 1; i > -1; i--) 
        {
            Layer* layer = layers [i];

            size_t M = layer -> size.M;
            size_t N = layer -> size.N;

            activation_fn fn_prime = layer -> fn_prime;

            // Gradient of loss function with respect to the nets of layer i
            for (int j = 0; j < M; j++) 
            {
                g [j] *= fn_prime (layer -> x [j]);
            };

            float b [M];
            float rgb [M];
            RegulariserGradientBiases (layer, rgb);

            float w [M][N];
            float** rgw = new float* [M];

            for (int j = 0; j < M; j++)
            {
                rgw [j] = new float [N];
            };

            RegulariserGradientWeights (layer, rgw);

            float* a;
            if (i > 0) 
            {
                a = layers [i - 1] -> activations;
            }

            else 
            {
                a = input;
            };

            // Calculate gradient of loss function with respect to the weights and biases of layer i
            for (int j = 0; j < M; j++)
            {
                b [j] = g [j] + regularisation_factor * rgb [j];

                for (int k = 0; k < N; k++)
                {
                    w [j][k] = g [j] * a [k] + regularisation_factor * rgw [j][k];
                };
            };


            #if DEBUG_LEVEL == 1
            std::cout << "layer " << i << " activations: ";
            for (int j = 0; j < M; j++) 
            {
                std::cout << layer -> activations [j] << " ";
            };
            std::cout << std::endl;

            std::cout << "layer " << i << " x:           ";
            for (int j = 0; j < M; j++) 
            {
                std::cout << layer -> x [j] << " ";
            };
            std::cout << std::endl;

            std::cout << "layer " << i << " g:           ";
            for (int j = 0; j < M; j++) 
            {
                std::cout << g [j] << " ";
            };
            std::cout << std::endl;

            std::cout << "bias gradient: ";
            for (int j = 0; j < M; j++) 
            {
                std::cout << b [j] << " ";
            };
            std::cout << std::endl;

            std::cout << "weights gradient: " << std::endl;
            for (int j = 0; j < M; j++) 
            {
                std::cout << "    ";
                for (int k = 0; k < N; k++)
                {
                    std::cout << w [j][k] << " ";
                };
                std::cout << std::endl;
            };
            std::cout << std::endl;
            #endif


            // Calculate gradient of loss function with respect to the activations of the previous layer (i - 1)
            float* x = new float [N];

            for (int k = 0; k < N; k++)
            {
                x [k] = 0.0;

                for (int j = 0; j < M; j++)
                {
                    x [k] += g [j] * layer -> weights [j][k];
                };
            };

            g = x;

            // Store the gradients
            for (int j = 0; j < M; j++)
            {
                bias_gradients [i][j] = b [j];

                for (int k = 0; k < N; k++)
                {
                    weight_gradients [i][j][k] = w [j][k];
                };
            };
        };
    };

    void UpdateWeightsAndBiases () 
    { 
        for (int i = 0; i < depth; i++)
        {
            Layer* layer = layers [i];
            
            size_t M = layer -> size.M;
            size_t N = layer -> size.N;

            for (int j = 0; j < M; j++)
            {
                layer -> biases [j] -= learning_rate * bias_gradients [i][j];

                for (int k = 0; k < N; k++)
                {
                    layer -> weights [j][k] -= learning_rate * weight_gradients [i][j][k];
                };
            };
        };
    };
    
};


void test_function (float x [4], float* y) 
{
    float sum = 0.0;
    for (int i = 0; i < 4; i++)
    {
        sum += (2 * x [i]) + 3;
        y [i] = sum;
    };
};


int main () 
{
    // Initialise Network
    size_t dimensions [] = {4, 5, 5, 4};
    activation_fn functions [] = {ReLU, ReLU, ReLU};
    activation_fn derivatives [] = {Step, Step, Step};
    float reg_factor = 0.0;
    float learn_rate = 0.01;

    Network <3> network (
        dimensions, 
        functions, 
        derivatives, 
        Identity, 
        MeanSquaredError, 
        MeanSquaredErrorGradient, 
        reg_factor, 
        learn_rate
    );


    // Create Fake Test Data
    size_t size = 1000;
    float dummy [1000][4];
    float* input [size];
    float* expected [size];

    std::mt19937 generator (1000);
    std::uniform_real_distribution <float> distribution (0.0, 1.0);

    for (int i = 0; i < size; i++) 
    {
        for (int j = 0; j < 4; j++)
        {
            dummy [i][j] = distribution (generator);
        };

        input [i] = dummy [i];

        float* y = new float [4];
        test_function (input [i], y);
        expected [i] = y;
    };


    // Train Network
    float* costs = network.TrainBatch (input, expected, size);


    // Process Results
    std::ofstream out;
    out.open ("losses.csv");

    for (int i = 0; i < size; i++) 
    {
        out << costs [i] << ",";
    };
    out.close ();

    system ("python graph.py");
};

// TODO: improve python graphing
// TODO: add scaling learning rate
// TODO: test regularisation
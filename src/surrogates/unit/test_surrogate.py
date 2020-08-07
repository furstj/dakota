import dakmod
#import matplotlib.pyplot as plt
import numpy as np

np.random.seed(44)

def simple_func(x):
    return x[0]*x[0] + 2.0*x[0]*x[1] + 3.0*x[1]
    
build_samples = -1.0 + 2*np.random.random((10,2))

build_response = np.zeros(10)
for i, x in enumerate(build_samples):
    build_response[i] = simple_func(x)

# This doesn't work due to the way NumPy is passed to Eigen (by copy)
# Working now with return value function


pr = dakmod.PolynomialRegression(build_samples, build_response, {"max degree" : 2, "scaler type" : "none"})
# JAS: keyword arguments would be more common than dict (and have to when providing defaults)
# foo(pos1, pos2, i=4, j=5)
# foo(P, Q, 3, 2)
# foo(pos2=Q, pos1=P, i=3, j=2)
##pr = dakmod.PolynomialRegression(build_samples, build_response,
##                                 maxdegree = 2, scalertype = None)

# JAS: Would be nice to call pr.fit() like sklearn...
#      Causes a nice separation between what's different per model being in ctor
#      vs. build which is generic
##pr = dakmod.PolynomialRegression({"max degree" : 2, "scaler type" : "none"})
##pr.build(build_samples, build_response);


eval_samples = -1.0 + 2*np.random.random((10,2))
eval_truth = np.zeros(10)
for i, x in enumerate(eval_samples):
    eval_truth[i] = simple_func(x)

eval_surr = pr.value(eval_samples)
print("Truth:\n{0}".format(eval_truth))
print("Surrogate value:\n{0}\n".format(eval_surr))

# deriv evaluation point
dpoint = np.atleast_2d(np.array([0.1, 0.4]))

eval_grad = pr.gradient(dpoint,0)
print("Surrogate gradient:\n{0}\n".format(eval_grad))

eval_hessian = pr.hessian(dpoint,0)
print("Surrogate Hessian:\n{0}\n".format(eval_hessian))


# Save Poly to text and then reload
# JAS: Save could be free function and a member on the model
# JAS: Could we enable Python pickling by delegating to C++ serialization
print("Saving Polynomial")
dakmod.save_poly(pr, "poly.txt", False)

# Demonstrate potential more Pythonic way to construct from file
print("Loading Polynomial")
prload = dakmod.PolynomialRegression(filename = "poly.txt", binary = False)

# JAS: consider more generic load function for arbitrary surrogate that casts to right type
#      could enable loading vector of surrogates of varying types and evaluate them
# JAS: could we always save/load to binary? text is portable, still not human readable
# DTS: What happens if you have pointer to Base, but some functions
#      available on one derived type but not another
##surrload = load("filename.txt", False)

assert(np.allclose(pr.value(dpoint), prload.value(dpoint)))


# This does work, and agrees
pypr = dakmod.PyPolyReg(build_samples, build_response, {"max degree" : 2, "scaler type" : "none"})
eval_pypr = pypr.value(eval_samples)
print("Truth:\n{0}".format(eval_truth))
print("Surrogate:\n{0}".format(eval_pypr))

# Try loading from file
# This reminds that we really need to load into pointer to get the type right
# Is this working correctly? Value output does not match

#pyprload = dakmod.PyPolyReg()
#dakmod.PyPolyReg.load("exported_poly.response_fn_1.txt", False, pyprload)

pyprload = dakmod.PolynomialRegression();
dakmod.load_poly("exported_poly.response_fn_1.txt", False, pyprload)

eval_loaded = pyprload.value(eval_samples)
print("Truth:\n{0}".format(eval_truth))
print("Loaded Surrogate:\n{0}".format(eval_loaded))

# try the GP
nugget_opts = {"estimate nugget" : True}
config_opts = {"scaler name" : "none", "Nugget" : nugget_opts }
gp = dakmod.GaussianProcess(build_samples, build_response, config_opts)

gp_eval_surr = gp.value(eval_samples)
print("Truth:\n{0}".format(eval_truth))
print("GP value:\n{0}\n".format(gp_eval_surr))

eval_grad = gp.gradient(dpoint,0)
print("GP gradient:\n{0}\n".format(eval_grad))

eval_hessian = gp.hessian(dpoint,0)
print("GP Hessian:\n{0}\n".format(eval_hessian))

# Save GP to binary and then reload
print("Saving GP")
dakmod.save_gp(gp, "gp.bin", True)

print("Loading GP")
gpload = dakmod.GaussianProcess();
dakmod.load_gp("gp.bin", True, gpload)
gpload_value = gpload.value(eval_samples)

assert(np.allclose(gpload_value, gp_eval_surr))

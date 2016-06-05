# Analyze results from batching experiments on linux, g++ 4.8.3.4, May 29, 2016.

import numpy as np
from collections import defaultdict
from matplotlib.pyplot import *


# Results we want to plot ...
files = ['identity', 'math', 'p1',
         'p2', 'p4', 'p6',
         'p8', 'p12', 'p16',
         'p20', 'p24', 'p28',
         'p32', 'p64', 'p128']

# ... in that order
algos = ['no batch', 'batch only', 'batch prefetch', 'locations batch']
colors = ['red', 'blue', 'green', 'orange']


def read_one_file(filename, give_all=False):
    """
    Read one result file and return numpy array of timings.
    """
    lines = open('linux.results/'+filename+'.txt', "rb").readlines()
    data = defaultdict(dict)
    
    for i, a_line in enumerate(lines):
        line = a_line.split()
        algo = line[0] + ' ' + line[1]
        batch_size = int(line[2])
        times = np.array([float(x) for x in line[3:]])
        if not give_all:
            data[algo][batch_size] = np.percentile(times, 50) / 1000  # milliseconds
        else:
            data[algo][batch_size] = times/1000

    return data


def plot_results_side_by_side():
    """
    Plot results for varying payloads side by side. 
    """
    
    ion()
    fig, axs = subplots(nrows=5, ncols=3)
    fig.set_figwidth(15)
    fig.set_size_inches(15, 11)
    n_obs = 40  # number of data points to display
    xlim = 80
    
    for i, filename in enumerate(files):
        data = read_one_file(filename)
        ax = axs[i/3][i%3]
        for c, algo in enumerate(algos):
            if algo == 'no batch':
                y = data[algo][0]
                ax.plot([0, xlim], [y, y], color=colors[c])
            else:
                x,y = data[algo].keys()[:n_obs], data[algo].values()[:n_obs]
                ax.plot(x, y, color=colors[c])
        props = dict(boxstyle='round', facecolor='wheat', alpha=0.3)
        ax.text(0.75, 0.23, filename, transform=ax.transAxes, fontsize=14,
                verticalalignment='top', bbox=props)
        #ax.set_title(filename, {'size': '11', 'weight': 'bold'})
        if i == 6:
            ax.set_ylabel("latency (ms)")
        if i == 13:
            ax.set_xlabel("batch size")
        #if i < 12:
        #    for x in ax.get_xticklabels():
        #        x.set_fontsize(0)
        #        x.set_visible(False)
        ax.set_xlim(2, xlim)
        ax.set_ylim(0, )
        ax.grid(True)

    fig.show()
    fig.suptitle("Latency (50th) as a function of batch size\nfor several payloads",
                 fontsize=18, weight='bold')
    fig.savefig('results-side-by-side.png')


def plot_speedups_for_payloads():
    """
    Plot the speedup as a function of the payload size.
    """
    ion()
    fig, axs = subplots(nrows=1, ncols=2)
    fig.set_figwidth(15)
    fig.set_size_inches(15, 9)
    bests = np.zeros(shape=(len(files)-2, 4))
    
    for i, filename in enumerate(files):
        if filename[0] != 'p':
            continue
        bests[i-2][0] = int(filename[1:])
        data = read_one_file(filename)
        y_no_batch = data["no batch"][0]
        for c, algo in enumerate(algos):
            if algo == 'no batch':
                continue
            else:
                x,y = data[algo].keys(), data[algo].values()
                best_i = np.argmin(y)
                best_x, best_y = x[best_i], y[best_i]
                speedup = float(y_no_batch) / float(best_y)
                bests[i-2][c] = speedup

    print bests
    ax = axs[0]
    for i in range(1,4):
        ax.plot(bests[:,0], bests[:,i], color=colors[i], label=algos[i])
    ax.grid(True)
    ax.set_xlim(2,128)
    ax.set_ylim(1,)
    ax.plot([4,4],[0,4.5], 'r--')
    ax.set_xticks([1,4,20,40,60,80,100,120])
    ax.set_xlabel("payload size")
    ax.set_ylabel("speedup")
    ax = axs[1]
    for i in range(1,4):
        ax.plot(bests[:,0], bests[:,i], color=colors[i], label=algos[i])
    ax.plot([4,4],[0,4.5], 'r--')
    ax.grid(True)
    ax.set_xlim(2,20)
    ax.set_ylim(1,)
    ax.legend()
    ax.set_xticks([1,4,5,10,15,20])
    ax.set_xlabel("payload size")
    ax.set_ylabel("speedup")
    fig.suptitle("Speedup (50th) as a function of payload", fontsize=18, weight='bold')
    fig.savefig('speedups-for-payloads.png')
    

def plot_speedups_for_p4():
    """
    Plot the speedup as a function of batch size.
    """
    ion()
    fig, axs = subplots(nrows=1, ncols=2)
    fig.set_figwidth(15)
    fig.set_size_inches(15, 9)
    data = read_one_file('p4')
    y_no_batch = data["no batch"][0]
    n_obs = 40  # number of data points to display
    xlim = 80
   
    for c, algo in enumerate(algos):
        for i in range(2):
            if algo == 'no batch':
                y = data[algo][0]
                axs[i].plot([0, xlim], [y, y], color=colors[c], label=algo)
            else:
                x,y = data[algo].keys()[:n_obs], data[algo].values()[:n_obs]
                axs[i].plot(x, y, label=algo, color=colors[c])
                #ax.set_ylabel("ms")
                
    for i in range(2):
        axs[i].grid(True)
        axs[i].set_ylim(0,)
        axs[i].set_xlabel("batch size")
    axs[0].set_ylabel("latency (ms)")
    axs[0].set_xlim(2,xlim)
    axs[1].set_xlim(2,32)
    
    algo = 'batch prefetch'
    x,y = data[algo].keys()[:n_obs], data[algo].values()[:n_obs]
    best_i = np.argmin(y)
    print x[best_i], y[best_i]
    ylim = axs[0].get_ylim()[1]
    axs[0].add_patch(Rectangle((8,0),12,ylim,facecolor='y', alpha=0.05, hatch='/'))
    axs[0].set_xticks([2,5,8,12,15,20,25,30,40,50,60,70,80])
    axs[0].plot([2,80], [24.719,24.719], 'r--', alpha=0.4)
    axs[0].plot([8,8], [0,ylim], 'r--', alpha=0.6)
    axs[0].plot([20,20], [0,ylim], 'r--', alpha=0.4)
    axs[0].plot([12,12], [0,ylim], 'r--', alpha=0.4)
    axs[1].add_patch(Rectangle((8,0),12,ylim,facecolor='y', alpha=0.05, hatch='/'))
    axs[1].plot([2,32], [24.719,24.719], 'r--', alpha=0.6)
    axs[1].plot([8,8], [0,ylim], 'r--', alpha=0.4)
    axs[1].plot([20,20], [0,ylim], 'r--', alpha=0.4)
    axs[1].plot([12,12], [0,ylim], 'r--', alpha=0.4)
    axs[1].plot([12,12], [22,27], 'r', alpha=1)
    axs[1].set_xticks([2,5,8,10,12,15,20,25,30])
    axs[1].set_yticks([0,20,24.719,30,35,40,60,80,100])
    annotate('4.1X', xy=(12,24.719),xytext=(17,15), fontsize=16, weight='bold',
             arrowprops=dict(facecolor='black', shrink=0.05, width=1))
    axs[1].legend()
    fig.suptitle("Latency (50th) as a function of batch size for p4", fontsize=18, weight='bold')
    fig.savefig('speedups-for-p4.png')
    

def plot_speedups_for_p4_with_deviations():
    """
    Plot p4 only, but with error bounds to give a sense of devitations.
    """
    ion()
    fig, axs = subplots(nrows=1, ncols=1)
    ax = axs
    fig.set_figwidth(11)
    fig.set_size_inches(11, 9)
    n_obs = 40  # number of data points to display
    xlim = 80
    data = read_one_file('p4', give_all=True)
    y_no_batch = data["no batch"][0]
    algo = 'batch prefetch'
    x,y = data[algo].keys()[:n_obs], data[algo].values()[:n_obs]
    speedups = y_no_batch / y
    M = np.max(speedups)
    ax.plot(x,speedups, color='gray', alpha=0.2)
    p25 = np.percentile(speedups, 25, axis=1)
    p50 = np.percentile(speedups, 50, axis=1)
    p75 = np.percentile(speedups, 75, axis=1)
    p05 = np.percentile(speedups, 5, axis=1)
    p95 = np.percentile(speedups, 95, axis=1)
    ax.plot(x, p05, 'b--', label='5th')
    ax.plot(x, p25, color='g', label='25th')
    ax.plot(x, p50, color='r', label='50th')
    ax.plot(x, p75, color='g', label='75th')
    ax.plot(x, p95, 'r--', label='95th')
    ax.fill_between(x, p05, p95, facecolor='yellow', alpha=0.1)
    ax.grid(True)
    ax.set_xlim(2,xlim)
    ax.set_ylim(2,)
    ax.legend(loc='upper right', fancybox=True, framealpha=0.3)
    ylim = ax.get_ylim()[1]
    ax.plot([12,12], [0,ylim], 'r--', alpha=0.4)
    ax.plot([2,xlim],[M,M], 'r--', alpha=0.4)
    ax.set_xticks([2,5,12,20,30,40,50,60,70,80])
    ax.set_yticks([2,2.5,3.0,3.5,4.0,4.28,4.5])
    ax.set_xlabel("batch size")
    ax.set_ylabel("speedup")
    #ax.set_title("Speedup variance")
    #ax = axs[1]
    #speedups2 = speedups - np.percentile(speedups, 50, axis=1).reshape(40,1)
    #speedups2 = speedups2.reshape(40*100)
    #ax.hist(speedups2,bins=50,orientation='horizontal')
    if False:  # overkill - but nice graphics :-)
        ax = axs[1]
        speedups50 = speedups - np.percentile(speedups, 50, axis=1).reshape(40,1)
        ax.plot(x, speedups50, color='gray', alpha=0.2)
        print speedups50.shape[1]
        for i in [0,10,70,80]: # range(0, speedups50.shape[1], 10):
            ax.plot(x, speedups50[:,i], color='g')
        p052 = np.percentile(speedups50, 5, axis=1)
        p952 = np.percentile(speedups50, 95, axis=1)
        ax.plot(x, p052, color='b',label='5th')
        ax.plot(x, p952, color='r', label='95th')
        ax.fill_between(x, p052, p952, facecolor='yellow', alpha=0.1)
        ax.legend(loc='lower right', fancybox=True, framealpha=0.3)
        #ax.hist(speedups.reshape(len(speedups)*100,1), bins=40)
        ax.grid(True)
        #ax.legend()
        ax.set_xlim(2,xlim)
        ax.set_title("Speedup variance, centered on 50th percentile")
    fig.suptitle("Speedup with variance\n(p4 payload - batch prefetch)", fontsize=18, weight='bold')
    fig.savefig('speedups-for-p4-with-deviations.png')

plot_results_side_by_side()
plot_speedups_for_payloads()
plot_speedups_for_p4()
plot_speedups_for_p4_with_deviations()

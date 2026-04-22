importScripts('bgp_sim.js');

let Module = null;

createBGPModule({
    print:    function() {},   // suppress stdout from C++ in browser console
    printErr: function() {}    // suppress stderr
}).then(function(m) {
    Module = m;
    postMessage({ type: 'ready' });
});

self.onmessage = function(e) {
    if (e.data.type !== 'simulate') return;

    const { caida, anns, rov, targetAsn } = e.data;

    try {
        const result = Module.ccall(
            'run_bgp_simulation',
            'string',
            ['string', 'string', 'string', 'number'],
            [caida, anns, rov, targetAsn >>> 0]   // >>> 0 forces unsigned 32-bit
        );
        postMessage({ type: 'result', data: result });
    } catch (err) {
        postMessage({ type: 'error', message: String(err) });
    }
};

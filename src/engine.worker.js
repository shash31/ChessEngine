import createChessEngineModule from './ChessEngine.js'

let engine = null;

// Initialize the engine inside the worker thread
createChessEngineModule({
  locateFile: (path) => path.endsWith('.wasm') ? `${import.meta.env.BASE_URL}ChessEngine.wasm` : path
}).then((module) => {
  engine = module;
  // Notify main thread that the worker engine is ready
  postMessage({ type: 'READY' });
});

// Listen for search requests from React
self.onmessage = (event) => {
  const { type, fen, depth } = event.data;

  if (type === 'SEARCH' && engine) {
    // Running heavy C++ search on background thread
    const bestMove = engine.ccall(
      'get_best_move',
      'string',
      ['string', 'number'],
      [fen, depth]
    );

    // Sending result back to React UI thread
    postMessage({ type: 'SEARCH_RESULT', bestMove });
  }
};


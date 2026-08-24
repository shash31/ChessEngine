import { useState, useRef, useEffect } from 'react';
import { Chessboard } from 'react-chessboard'
import { Chess } from 'chess.js';
import './styles.css'

// Todos:
// - Set up web workers eventually maybe

function App() {
  const chessGameRef = useRef(new Chess());
  const chessGame = chessGameRef.current;

  const [chessPosition, setChessPosition] = useState(chessGame.fen());
  const [depth, setDepth] = useState(4);

  const [isSearching, setIsSearching] = useState(false);
  const workerRef = useRef(null);

  useEffect(() => {
    workerRef.current = new Worker(
      new URL('./engine.worker.js', import.meta.url),
      { type: 'module' }
    );

    workerRef.current.onmessage = (event) => {
      const { type, bestMove } = event.data;

      if (type === 'READY') {
        console.log('[Worker] Chess Engine Ready!');
      } else if (type === 'SEARCH_RESULT') {
        const fromSq = bestMove & 0x3F;        // Bits 0-5
        const toSq = (bestMove >> 6) & 0x3F;   // Bits 6-11
        const flags = bestMove >> 12;          // Bits 12-15
        const promotionPiece = (flags >= 8) ? ['n', 'b', 'r', 'q'][(flags - 8)%4] : null;

        const fileChar = (sq) => String.fromCharCode('a'.charCodeAt(0) + (sq % 8));
        const rankChar = (sq) => String((sq >> 3) + 1);

        chessGame.move({
          from: fileChar(fromSq) + rankChar(fromSq),
          to: fileChar(toSq) + rankChar(toSq),
          promotion: promotionPiece
        });

        setChessPosition(chessGame.fen());
        
        setIsSearching(false);
      }
    };

    return () => workerRef.current?.terminate();
  }, []);

  const triggerEngineMove = (currentFen, searchDepth = depth) => {
    if (!workerRef.current || isSearching) return;
    
    setIsSearching(true);
    workerRef.current.postMessage({
      type: 'SEARCH',
      fen: currentFen,
      depth: searchDepth
    });
  };

  function onPieceDrop({ sourceSquare, targetSquare }) {
    console.log(sourceSquare);
    console.log(targetSquare);

    if (isSearching) return false; // Prevent user from making moves while engine is thinking
    if (!targetSquare) return;

    try {
      chessGame.move({
        from: sourceSquare,
        to: targetSquare,
        promotion: 'q'
      });

      setChessPosition(chessGame.fen());

      triggerEngineMove(chessGame.fen(), depth);

      return true;
    } catch (error) {
      return false;
    }
  }

  function undoMove() {
    chessGame.undo();
    setChessPosition(chessGame.fen());
  }

  function newGame() {
    chessGame.reset();
    setChessPosition(chessGame.fen());
  }

  const chessBoardOptions = {
    onPieceDrop,
    position: chessPosition
  }

  return (
    <main>
        <Chessboard options={chessBoardOptions}/>
        <div id="settings">
          <label>Depth: </label>
          <input type="number" value={depth} min="1" onChange={(e) => setDepth(parseInt(e.target.value))} />
          <button id="undo-button" onClick={undoMove}>Undo Move</button>
          <button id="new-game-button" onClick={newGame}>New Game</button>
          <button id="engine-move-button" onClick={() => triggerEngineMove(chessGame.fen(), depth)}>Engine Move</button>
        </div>
        <div id="status">
          {isSearching ? "Engine is thinking..." : "Engine is idle."}
        </div>
    </main>
  )
}

export default App;

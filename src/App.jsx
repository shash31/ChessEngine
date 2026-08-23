import { useState, useRef, useEffect } from 'react';
import { Chessboard } from 'react-chessboard'
import { Chess } from 'chess.js';
import createChessEngineModule from './ChessEngine.js'
import './styles.css'

// Todos:
// - Set up web workers eventually maybe

function App() {
  const chessGameRef = useRef(new Chess())
  const chessGame = chessGameRef.current

  const [chessPosition, setChessPosition] = useState(chessGame.fen());
  const [engine, setEngine] = useState(null);
  const [depth, setDepth] = useState(3);

  useEffect(() => {
    createChessEngineModule({
      locateFile: (path) => path.endsWith('.wasm') ? '/ChessEngine.wasm' : path
    }).then((module) => {
      setEngine(module);
    });
  }, []);

  function onPieceDrop({ sourceSquare, targetSquare }) {
    console.log(sourceSquare);
    console.log(targetSquare);

    if (!targetSquare) return;

    try {
      chessGame.move({
        from: sourceSquare,
        to: targetSquare,
        promotion: 'q'
      });

      setChessPosition(chessGame.fen());

      console.log('sending move');
      const move = engine.ccall(
        'get_best_move',
        'string',
        ['string', 'number'], // Types: string, integer
        [chessGame.fen(), depth]  // Values passed to C++
      );
      console.log('engine returned move');
      console.log(move);

      chessGame.move({
        from: move.substring(0, 2),
        to: move.substring(2, 4),
        promotion: 'q'
      });

      setChessPosition(chessGame.fen());

      return true;
    } catch (error) {
      return false;
    }
  }

  function undoMove() {
    chessGame.undo();
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
          <input type="number" value={depth} onChange={(e) => setDepth(parseInt(e.target.value))} />
          <button id="undo-button" onClick={undoMove}>Undo Move</button>
        </div>
    </main>
  )
}

export default App;

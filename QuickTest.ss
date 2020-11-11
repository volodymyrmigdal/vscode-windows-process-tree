( function _QuickTest_() {
  
  let assert = require( 'assert' );
  let ProcessTreeWindows = require( './lib/index.js' );
  let ctime = ProcessTreeWindows.getProcessCreationTime( process.pid );
  assert.notStrictEqual( ctime, null );
})();
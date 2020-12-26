( function _QuickTest_() {

  let assert = require( 'assert' );
  let ProcessTreeWindows = require( './lib/index.js' );
  let ctime = ProcessTreeWindows.getProcessList( process.ppid, ( list ) =>
  {
    console.log( list )
  });
  // assert.notStrictEqual( ctime, null );
})();
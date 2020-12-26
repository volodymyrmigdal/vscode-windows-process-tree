( function _Int_test_s( )
{

'use strict';

let ProcessTreeWindows;

if( typeof module !== 'undefined' )
{
  let _ = require( 'wTools' );
  _.include( 'wTesting' );
  _.include( 'wFiles' );
  _.include( 'wProcess' );
  _.include( 'wProcessWatcher' );

  ProcessTreeWindows = require( '../lib/index.js' );
}

let _global = _global_;
let _ = _global_.wTools;
let Self = {};

// --
// context
// --

function suiteBegin()
{
  let context = this;
}

//

function suiteEnd()
{
  let context = this;
}

// --
//
// --

function _childrenWindows( test )
{
  let context = this;

  if( process.platform !== 'win32' )
  {
    test.identical( 1, 1 );
    return;
  }

  let mainTree =
  {
    executionTime : [ 50, 100 ],
    spawnPeriod : [ 25, 50 ],
  }

  let additionalReady = runAdditionalTrees( 5 )
  let mainReady = _.process._startTree( mainTree );
  let snapshots = [];

  let timer = _.time.periodic( 10, () =>
  {
    if( !_.process.isAlive( mainTree.rootOp.pnd.pid ) )
    return true;

    ProcessTreeWindows.getProcessList( mainTree.rootOp.pnd.pid, ( snapshot ) =>
    {
      if( snapshot.length > 1 )
      snapshots.push( snapshot )
    })

    return true;
  })

  mainReady
  .then( async () =>
  {
    timer.cancel();

    console.log( 'Main tree:', _.toJs( mainTree.list ) )

    let status = checkSnapshots();

    test.identical( status, true );

    await additionalReady;

    return status;
  })

  return mainReady;

  /* */

  function checkSnapshots()
  {
    for( let s = 0; s < snapshots.length; s++ )
    {
      let snapshot = snapshots[ s ];
      for( let i = 0; i < snapshot.length; i++ )
      {
        let targetPnd = snapshot[ i ];

        let nodeIs = targetPnd.name === 'node.exe';

        if( !nodeIs )
        {
          console.error( 'Unexpected process in tree.\n' + processInfo( snapshot, targetPnd ) );
          return false;
        }

        let result = mainTree.list.filter( ( treePnd ) =>
        {
          return treePnd.pid === targetPnd.pid;
        })

        let mainTreeHasPid = result.length === 1;

        if( !mainTreeHasPid )
        {
          console.error( `Main tree does not have pnd: ${targetPnd}` );
          return false;
        }

        let resultPnd = result[ 0 ];

        let ppidsAreSame = resultPnd.ppid === targetPnd.ppid;

        if( !ppidsAreSame )
        {
          console.error( `Ppid changed.\nTree pnd: ${_.toJs( resultPnd )}\nSnapshot pnd: ${_.toJs( targetPnd )}` )
          return false;
        }
      }
    }

    return true;
  }

  /* */

  function processInfo( snapshot, targetPnd )
  {
    let msg =
`
Process in snapshot: ${_.toJs( targetPnd ) }
Process parent in snapshot: ${_.toJs( findParentFor( snapshot, targetPnd ) ) }
Process in main tree: ${_.toJs( findProcess( mainTree.list, targetPnd ) ) }
Process parent in main tree: ${_.toJs( findParentFor( mainTree.list, targetPnd ) ) }
`
  return msg;
  }

  function findParentFor( src, targetPnd )
  {
    let result = src.filter( ( pnd ) =>
    {
      return pnd.pid === targetPnd.ppid;
    })
    return result;
  }

  function findProcess( src, targetPnd )
  {
    let result = src.filter( ( pnd ) =>
    {
      return pnd.pid === targetPnd.pid;
    })
    return result;
  }

  /* */

  function runAdditionalTrees( n )
  {
    let cons = [];
    for( let i = 0; i < n; i++ )
    {
      let tree =
      {
        executionTime : [ 25, 100 ],
        spawnPeriod : [ 25, 50 ],
        max : 10
      }
      _.process._startTree( tree );
      cons.push( tree.ready )
    }
    return _.Consequence.AndKeep( ... cons );
  }
}

//

function getProcessList( test )
{
  let context = this;

  let timeLimit = context.t60 * 10;
  let begin = _.time.now();

  let ready = _childrenWindows.call( context, test );

  ready.then( ( op ) =>
  {
    let spent = _.time.now() - begin;
    if( spent >= timeLimit )
    return op;

    return _childrenWindows.call( context, test );
  })

  return ready;
}

getProcessList.routineTimeOut = 60000 * 15;

//

var Proto =
{

  name : 'wProcessTreeWindows',
  silencing : 1,
  routineTimeOut : 60000,
  onSuiteBegin : suiteBegin,
  onSuiteEnd : suiteEnd,

  context :
  {
    suiteTempPath : null,
    assetsOriginalPath : null,
    appJsPath : null,
    t60 : 60000
  },

  tests :
  {
    getProcessList
  }

}

_.mapExtend( Self, Proto );

//

Self = wTestSuite( Self );

if( typeof module !== 'undefined' && !module.parent )
wTester.test( Self )

})();

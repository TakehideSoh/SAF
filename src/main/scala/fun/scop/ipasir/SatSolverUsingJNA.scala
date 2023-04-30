package fun.scop.ipasir

import java.util

import com.sun.jna.ptr.IntByReference
import com.sun.jna.{Callback, Library, Native, Pointer, ptr}
import fun.scop.sat._

import scala.collection.mutable.ArrayBuffer


class UserCallback() extends Callback {
  def invoke(state: IntByReference): Int = {
    return if (state.getPointer().getInt(0) != 0) 1 else  0
  }
}


case class SatSolverUsingJNA(name: String) {

  trait IPASIR extends Library {
    def ipasir_signature: String

    def ipasir_init: Pointer

    def ipasir_release(solver: Pointer): Unit

    def ipasir_add(solver: Pointer, lit_or_zero: Int): Unit

    def ipasir_assume(solver: Pointer, lit: Int): Unit

    def ipasir_solve(solver: Pointer): Int

    def ipasir_val(solver: Pointer, lit: Int): Int

    def ipasir_failed(solver: Pointer, lit: Int): Int

    def ipasir_set_terminate(solver: Pointer, state: Pointer, callback: Callback): Unit
  }

  trait ISATLIB extends Library {
    def isat_add_clauses(solver: Pointer, clause: Array[Int], length: Int): Unit

    def isat_vars(solver: Pointer): Int

    def isat_model(solver: Pointer): IntByReference

    def isat_clean(solver: Pointer, p: Pointer): Unit

    def isat_set_verbosity(solver: Pointer, level: Int): Unit

    def isat_set_nof_threads(solver: Pointer, nsolvers: Int): Unit

    def isat_set_rndseed(solver: Pointer, rndseed: Int): Unit

    def isat_set_rndactivation(solver: Pointer, threadId: Int): Unit

    def isat_print_unaryLearntClauses(solver: Pointer): Unit

    def isat_print_binaryLearntClauses(solver: Pointer): Unit

  }



  //val map =  new util.HashMap[String,Int]()
  //map.put(Library.OPTION_OPEN_FLAGS,0)
  val solverName = name
  // val ipasirLib = Native.load(solverName, classOf[IPASIR_EXT], map)

  println("JNA library load: start")
  val ipasirLib: IPASIR = Native.load(solverName, classOf[IPASIR])
  val isatLib: ISATLIB = Native.load(solverName, classOf[ISATLIB])
  println("JNA library load: end")
  // println(System.getProperty("java.library.path"))
  val solverPtr = ipasirLib.ipasir_init

  var bufsize = 1000

  val callback = new UserCallback
  val state = new IntByReference()
  val statePtr = state.getPointer
  setTerminate(0)

  def getSignature: String = ipasirLib.ipasir_signature

  def init: SatSolverUsingJNA = {
    val solver = new SatSolverUsingJNA(solverName)
    solver
  }

  def release(): Unit = {
    ipasirLib.ipasir_release(solverPtr)
  }

  def add(lit_or_zero: Int): Unit = {
    ipasirLib.ipasir_add(solverPtr, lit_or_zero)
  }

  def setNsolvers(nsolvers: Int): Unit = {
    println(s"call setNsolvers(nsolvers: Int)")
    isatLib.isat_set_nof_threads(solverPtr, nsolvers)
  }

  def setRndSeed(rndseed: Int): Unit = {
    println(s"call setRndSeed(rndseed: Int)")
    isatLib.isat_set_rndseed(solverPtr, rndseed)
  }

  def setRndActivation(index: Int): Unit = {
    println(s"call setRndActivation(index: Int)")
    isatLib.isat_set_rndactivation(solverPtr, index)
  }

  def printUnaryLearntClauses() = {
    println(s"call printUnaryLearntClauses()")
    isatLib.isat_print_unaryLearntClauses(solverPtr)
  }

  def printBinaryLearntClauses() = {
    println(s"call printBinaryLearntClauses()")
    isatLib.isat_print_binaryLearntClauses(solverPtr)
  }

  def assume(lit: Int): Unit = {
    ipasirLib.ipasir_assume(solverPtr, lit)
  }

  def solve(): Int = {
    done()
    // System.out.println("=========== TEST SAT Solver solve 0 =========== ");
    ipasirLib.ipasir_solve(solverPtr)
  }

  def `val`(lit: Int): Int = ipasirLib.ipasir_val(solverPtr, lit)

  def failed(lit: Int): Int = ipasirLib.ipasir_failed(solverPtr, lit)

  def setTerminate(value: Int): Unit = {
    if (value == 0) ipasirLib.ipasir_set_terminate(solverPtr, null, null)
    else {
      state.setValue(value)
      ipasirLib.ipasir_set_terminate(solverPtr, statePtr, callback)
    }
  }

  def setBufferSize(size: Int): Unit = {
    bufsize = size
  }

  def setVerbosity(verbosity: Int): Unit = {
    isatLib.isat_set_verbosity(solverPtr, verbosity)
  }

  def setVars(count: Int): Unit = {
  }

  def addConcatenatedClauses(clause: Array[Int]): Unit = { // System.out.println("+++++++++++++++++++++++++++++++++++");
    isatLib.isat_add_clauses(solverPtr, clause, clause.size)
  }

  val bufferOfClauses = ArrayBuffer.empty[Array[Int]]
  var numberOfBufferedClauses = 0

  private def flushBuffer(): Array[Int] = {
    val concatenationOfClauses = new Array[Int](bufferOfClauses.map(clause => clause.size + 1).sum)
    var k = 0
    for (clause <- bufferOfClauses) {
      for (i <- 0 until clause.size)
        concatenationOfClauses(k + i) = clause(i)
      concatenationOfClauses(k + clause.size) = 0
      k += clause.size + 1
    }
    bufferOfClauses.clear()
    concatenationOfClauses
  }

  def addClause(lits: Array[Int]): Unit = {
    bufferOfClauses.append(lits.clone)
    numberOfBufferedClauses += 1
    if (numberOfBufferedClauses == bufsize) {
      val clauses = flushBuffer()
      addConcatenatedClauses(clauses)
      numberOfBufferedClauses = 0
    }
  }

  def addClauses(clauses: Array[Array[Int]]): Unit = {
    for (clause <- clauses)
      addClause(clause)
  }

  def done(): Unit = {
    if (numberOfBufferedClauses == 0) return
    val clauses = flushBuffer()
    addConcatenatedClauses(clauses)
    numberOfBufferedClauses = 0
  }

  def solve(assumptions: Array[Int]): Int = {
    if (assumptions != null) for (lit <- assumptions) {
      ipasirLib.ipasir_assume(solverPtr, lit)
    }
    solve
  }

  def getModel(): Array[Int] = {
    val length = isatLib.isat_vars(solverPtr);
    val model = new Array[Int](length)

    val modelPtr = isatLib.isat_model(solverPtr)

    val p = modelPtr.getPointer()

    for (i <- 0 until length)
      model(i) = p.getInt(i * 4)

    isatLib.isat_clean(solverPtr, p)

    return model
  }


}

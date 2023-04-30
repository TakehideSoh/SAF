package fun.scop.ipasir

import java.util

import com.sun.jna.ptr.IntByReference
import com.sun.jna.{Callback, Library, Native, Pointer, ptr}
import fun.scop.sat._

import scala.collection.mutable.ArrayBuffer

case class SatSolverUsingPureIpasir(name: String) {

  trait IPASIR extends Library {
    def ipasir_signature: String

    def ipasir_init: Pointer

    def ipasir_release(solver: Pointer): Unit

    def ipasir_add(solver: Pointer, lit_or_zero: Int): Unit

    def ipasir_add_lits(solver: Pointer, lits: Array[Int], litssize: Int): Unit

    def ipasir_assume(solver: Pointer, lit: Int): Unit

    def ipasir_solve(solver: Pointer): Int

    def ipasir_val(solver: Pointer, lit: Int): Int

    def ipasir_failed(solver: Pointer, lit: Int): Int

    def ipasir_set_terminate(
        solver: Pointer,
        state: Pointer,
        callback: Callback
    ): Unit
  }

  val solverName = name
  // println("JNA library load: start")
  val ipasirLib: IPASIR = Native.load(solverName, classOf[IPASIR])
  // println("JNA library load: end")

  val solverPtr = ipasirLib.ipasir_init

  val callback = new UserCallback
  val state = new IntByReference()
  val statePtr = state.getPointer
  setTerminate(0)

  def getSignature: String = ipasirLib.ipasir_signature

  // def init: SatSolverUsingPureIpasir = new SatSolverUsingPureIpasir(solverName)

  def release(): Unit = {
    ipasirLib.ipasir_release(solverPtr)
  }

  def add(lit_or_zero: Int): Unit = {
    ipasirLib.ipasir_add(solverPtr, lit_or_zero)
  }

  private def add(lits: Array[Int]): Unit = {
    ipasirLib.ipasir_add_lits(solverPtr, lits, lits.size)
  }

  def assume(lit: Int): Unit = {
    ipasirLib.ipasir_assume(solverPtr, lit)
  }

  def solve(): Int = {
    done()
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

  val bufferOfLits = ArrayBuffer.empty[Int]
  var numberOfBufferedClauses = 0
  var bufsize = 10000

  def addLit(lit: Int): Unit = {
    bufferOfLits.append(lit)
    if (bufferOfLits.size == bufsize) {
      add(bufferOfLits.toArray)
      bufferOfLits.clear()
    }
  }

  private def done(): Unit = {
    if (bufferOfLits.size == 0) return
    else {
      add(bufferOfLits.toArray)
      bufferOfLits.clear()
    }
  }

  def solve(assumptions: Array[Int]): Int = {
    done()
    if (assumptions != null) for (lit <- assumptions) {
      ipasirLib.ipasir_assume(solverPtr, lit)
    }
    solve
  }

}

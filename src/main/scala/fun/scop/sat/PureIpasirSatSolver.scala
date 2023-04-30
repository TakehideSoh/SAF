package fun.scop.sat

import com.sun.jna.NativeLibrary
import fun.scop.ipasir.SatSolverUsingPureIpasir

class PureIpasirSatSolver(libraryName: String, libraryPath: String)
    extends SatSolver {

  private var modelArray: Option[Array[Int]] = None

  val satSolver = load

  def load = {
    // println(s"PureIpasirSatSolver using $libraryName @ $libraryPath")
    NativeLibrary.addSearchPath(libraryName, libraryPath)
    SatSolverUsingPureIpasir(libraryName)
  }

  def init = {}

  private[this] def makeReturnValue(code: Int): Option[Boolean] = {
    if (code == 10) {
      // println("c SAT")
      Some(true)
    } else if (code == 20) {
      // println("c UNSAT")
      Some(false)
    } else {
      None
    }
  }

  override def solve: Option[Boolean] = {

    /** ipasir では 0 is undef 10 is satisfiable (SAT) 20 is unsatisfiable (UNSAT)
      */
    val resultNum = satSolver.solve()

    makeReturnValue(resultNum)
  }

  def solve(assumptions: Seq[Int]): Option[Boolean] = {
    val resultNum = satSolver.solve(assumptions.toArray)

    makeReturnValue(resultNum)
  }

  override def model(v: Int): Int = satSolver.`val`(v)

  override def addClause(lits: Seq[Int]): Unit = {
    for (lit <- lits)
      satSolver.addLit(lit.toInt)
    satSolver.addLit(0)
  }

  override def addComment(str: String): Unit = {}
}

object PureIpasirSatSolverTest {

  def getBlockClause(lits: Seq[Int]) = {
    lits.map(_ * -1)
  }

  def main(args: Array[String]): Unit = {

    val Array(jnalibname, jnalibpath) = args

    val s = new PureIpasirSatSolver(jnalibname, jnalibpath)

    s.addClause(Seq(1, 2, 3))
    s.addClause(Seq(-1, -2))
    s.addClause(Seq(-2, -3))
    s.addClause(Seq(-1, -3))

    while (s.solve.get) {
      println(s"${s.model(1)},${s.model(2)},${s.model(3)}")
      val blk = getBlockClause(Seq(1, 2, 3).map(s.model(_)))
      s.addClause(blk)
    }
  }
}

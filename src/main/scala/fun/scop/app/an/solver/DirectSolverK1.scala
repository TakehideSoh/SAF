package fun.scop.app.an.solver

import fun.scop.app.an.util._

import fun.scop.sat._

class SafK1(
    an: AutomataNetwork,
    solverpath: String,
    verbose: Boolean = true
) extends AbstractSolver() {

  var dimacsIndex = 0
  var nofClauses = 0

  var xx: Map[(Automata, Int, Int), Int] = Map.empty
  var dmap: Map[Int, String] = Map.empty

  var clauses: Seq[String] = Seq.empty

  private def issueAuxVar() = {
    dimacsIndex += 1
    dimacsIndex
  }

  private def makeAutomataVar(k: Int) = {
    for (a <- an.automatas; s <- 1 to k) {
      if (an.domain(a).size > 2) {
        for (v <- an.domain(a)) {
          dimacsIndex += 1
          xx += (a, s, v) -> dimacsIndex
          dmap += dimacsIndex -> s"x(${a.name},$s)=$v"
        }
      } else {
        dimacsIndex += 1
        xx += (a, s, 0) -> -dimacsIndex
        xx += (a, s, 1) -> dimacsIndex
        dmap += dimacsIndex -> s"x(${a.name},$s)=1"
      }
    }
  }

  private def x(a: Automata, i: Int, v: Int) = xx(a, i, v)

  private def atMostOne(ps: Seq[Int]): Seq[Seq[Int]] =
    ps.map(_ * -1).combinations(2).toSeq

  private def atLeastOne(ps: Seq[Int]): Seq[Seq[Int]] = Seq(ps)

  private def exactOne(ps: Seq[Int]): Seq[Seq[Int]] =
    atMostOne(ps) ++ atLeastOne(ps)

  private def addAllClauses(cs: Seq[Seq[Int]]) =
    cs.foreach(addClause)

  private def addClause(lits: Seq[Int]) = {
    if (false) {
      val ss = for {
        l <- lits
        p = math.abs(l)
        str = if (l < 0) s"-${dmap(p)}" else s"${dmap(p)}"
      } yield str
    }
    nofClauses += 1
    clauses = s"${lits.mkString(" ")} 0" +: clauses
  }

  private def makeIntVarEncoding(k: Int) = {
    for (a <- an.automatas; s <- 1 to k if an.domain(a).size > 2)
      DirectVarEncoding(a, s)
  }

  private def DirectVarEncoding(a: Automata, s: Int) =
    addAllClauses(exactOne(an.domain(a).map(d => x(a, s, d))))

  private def makeOriginConstraint(k: Int) = {
    for (tr <- an.transitions; i <- 1 to k)
      addClause(tr.pre.map(av => -x(av.a, i, av.v)).toSeq)
  }

  private def encode(k: Int, foundSoFar: Seq[Attractor]) = {
    /* defining variables */
    makeAutomataVar(k)

    /* clauses of definition */
    makeIntVarEncoding(k)

    /* clauses of constraints */
    makeOriginConstraint(k)

  }

  private def makeFile(file: String) = {
    import java.io.PrintWriter

    val out = new PrintWriter(file)
    out.write(s"p cnf ${dimacsIndex} ${nofClauses}\n")
    for (lits <- clauses)
      out.write(lits + "\n")
    out.close()

  }

  private def execSolver(file: String) = {
    import scala.sys.process.Process

    val process = Process(
      s"$solverpath $file ${if (verbose) "out" else ""}"
    ).run

    if (process.exitValue() == 0) {
      Process(s"rm $file")
      // println("Exit 0.")
    }
  }

  def findAttractorsLeK(
      k: Int,
      attractorsFoundSoFar: Seq[Attractor]
  ): Seq[Attractor] = {

    encode(k, attractorsFoundSoFar)

    val tmpFile = "/tmp/tmp.cnf"

    makeFile(tmpFile)

    // for (i <- dmap.keys.toSeq.sorted) {

    //   println(s"$i: ${dmap(i)}")
    // }
    execSolver(tmpFile)

    Seq.empty
  }

}

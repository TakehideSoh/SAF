package fun.scop.sat

import java.io.IOException
import java.lang.management.ManagementFactory

import scala.concurrent.ExecutionContext.Implicits.global
import scala.concurrent._
import scala.concurrent.duration._
import scala.io.Source
import scala.sys.process.Process

abstract class SatSolver() extends SatProblem {
  def init: Unit

  def solve: Option[Boolean]

  def solve(assumptions: Seq[Int]): Option[Boolean]

  def model(v: Int): Int

  //  def whoami: String
}

//case class debugProblem(toSTDOUT: Boolean) extends SatProblem {
//  var clauses: IndexedSeq[Seq[String]] = IndexedSeq.empty
//  def addClause(lits: Seq[Int]) {
//    if (toSTDOUT) println(lits.map(i => OrderEncode.naming(i)).mkString(" "))
//    clauses = clauses :+ lits.map(i => OrderEncode.naming(i)).toSeq
//  }
//  def addComment(str: String) {
//    if (toSTDOUT) println(str)
//  }
//}

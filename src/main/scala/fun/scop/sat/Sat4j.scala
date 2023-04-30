package fun.scop.sat

case class Sat4j(option: String = "default") extends SatSolver {

  import org.sat4j.core.VecInt
  import org.sat4j.minisat.SolverFactory
  import org.sat4j.minisat.core.{Solver => MinisatSolver}
  import org.sat4j.tools.{DimacsStringSolver, ModelIterator}

  // def this() = this("default")

  val sat4j = option.capitalize match {
    case "Iterator" => new ModelIterator(SolverFactory.newDefault)
    case "Dimacs"   => new DimacsStringSolver
    case name       => SolverFactory.instance.createSolverByName(name)
  }

  var statmap = sat4j.getStat

  def init = sat4j.reset()

  private def minisat: MinisatSolver[_] = sat4j match {
    case x: MinisatSolver[_] => x
    case _ => sys.error("org.sat4j.minisat.core.Solver was expected")
  }

  override def addClause(lits: Seq[Int]): Unit = {
    // println(lits.mkString(" "))
    sat4j.addClause(new VecInt(lits.map(_.toInt).toArray))
  }

  override def addComment(str: String): Unit = {}

  def isSatisfiable = {
    sat4j.isSatisfiable
    statmap = sat4j.getStat
  }

  def isSatisfiable(assumps: Seq[Int]) = {
    sat4j.isSatisfiable(new VecInt(assumps.toArray))
    statmap = sat4j.getStat
  }

  def solve = {
    val res = Some(sat4j.isSatisfiable)
    statmap = sat4j.getStat
    res
  }

  def solve(assumptions: Seq[Int]): Option[Boolean] = {
    val res = Some(sat4j.isSatisfiable(new VecInt(assumptions.toArray)))
    statmap = sat4j.getStat
    res
  }

  // def model: Array[Int] = sat4j.model
  def model(v: Int): Int = {
    if (sat4j.model(v)) v else -v
  }

}
